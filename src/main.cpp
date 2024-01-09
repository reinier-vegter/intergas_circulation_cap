/*
  Intergas circulation (water) pump cap to limit the noise of this huge pump.
  Probably only usable in case of a bundled Daikin hybrid heatpump.

  It might happen that the heatpump configures the boiler so that it will ignore
  the configured maximum pump capacity, which induces a lot of noise in radiators.

  This Arduino project is meant to intercept the PWM signal wire from the boiler to the Wilo pump,
  and cap its capacity to X%, integrating with Home Assistant through Zigbee2MQTT.

  Obviously use at your own risk.
 */
#include <Arduino.h>
#include <SPI.h>
#include <WiFiNINA.h>
#include <ArduinoHA.h>
#include <pwm.h>
#include <relay.h>

#include "config.h"
const char HA_DEVICE_SOFTWARE_VERSION[] = "1.0.4";

// Put configuration in include/config.h
const char ssid[] = SECRET_SSID;
const char pass[] = SECRET_PASS;
int default_cap_pct = DEFAULT_CAP_PCT;
int pwm_in_pin = PWM_IN_PIN;
int pwm_out_pin = PWM_OUT_PIN;
int relay_out_pin = RELAY_OUT_PIN;
char ha_device_name[] = HA_DEVICE_NAME;
IPAddress ip;
unsigned long currentMillis = millis();

int wifiStatus = WL_IDLE_STATUS;
WiFiClient client;
unsigned long wifiCheckInterval = 20000;
unsigned long wifiCheckpreviousMillis = 0;
int pingResult;
bool connectionProblem = false;
int connectionProblemCount = 0;
int connectionProblemRebootTheshold = 100;

unsigned long pwmSetInterval = 3000;
unsigned long pwmSetPreviousMillis = 0;
int currentPumpCap = 100;

// Nr of sensors to send to MQTT/HA.
const int MQTT_MAX_DEVICE_NR = 6;

// HA integration.
byte mac[] = {0x00, 0x10, 0xFA, 0x6E, 0x38, 0x4A};
HADevice *device;
HAMqtt *mqtt;

// Cap prct
HANumber *cvPumpCap;
HASensorNumber *cvPumpInputPwm;
HASensorNumber *cvPumpOutputPwm;
HASensorNumber *cvPumpInputPct;
HASensorNumber *cvPumpOutputPct;
HASwitch *switch1;  

// Standard reset function
void(* resetFunc) (void) = 0;

void printWifiStatus()
{
  if (WiFi.status() == WL_CONNECTED) {
    // print the SSID of the network you're attached to:
    Serial.print(F("Connected to SSID: "));
    Serial.println(WiFi.SSID());

    // print your board's IP address:
    ip = WiFi.localIP();
    Serial.print(F("IP Address: "));
    Serial.println(ip);

    // print the received signal strength:
    long rssi = WiFi.RSSI();
    Serial.print(F("signal strength (RSSI):"));
    Serial.print(rssi);
    Serial.println(F(" dBm"));
  } else {
    Serial.print(F("Wifi connection failed, status: "));
    Serial.println(WiFi.status());
  }
}

void onNumberCommand(HANumeric number, HANumber *sender)
{
  Serial.print(F("Cap updated from MQTT: "));
  Serial.println(number.getBaseValue());
  sender->setState(number); // report the selected option back to the HA panel
}

void onSwitchCommand(bool state, HASwitch* sender) {
  sender->setState(setRelay(state)); // report state back to the Home Assistant
}

void setupHA() {
  device = new HADevice(mac, sizeof(mac));
  mqtt = new HAMqtt(client, *device, MQTT_MAX_DEVICE_NR + 1);

  // Cap prct
  cvPumpCap = new HANumber("cvCirculationPumpCap");
  cvPumpInputPwm = new HASensorNumber("cvPumpInput");
  cvPumpOutputPwm = new HASensorNumber("cvPumpOutput");
  cvPumpInputPct = new HASensorNumber("cvPumpInputPct");
  cvPumpOutputPct = new HASensorNumber("cvPumpOutputPct");

  if (AD_RELAY_ENABLED) {
    Serial.println(F("Enabling addition relay"));
    switch1 = new HASwitch("cvSwitch1");  
  }
}

void setupHASensors() {
  // set device's details (optional)
  device->setName(ha_device_name);
  device->setSoftwareVersion(HA_DEVICE_SOFTWARE_VERSION);

  device->enableSharedAvailability();
  device->enableLastWill();

  // handle command from the HA panel
  cvPumpCap->onCommand(onNumberCommand);

  // Optional configuration
  cvPumpCap->setIcon("mdi:gauge");
  cvPumpCap->setName("CV Circulation pump cap");
  cvPumpCap->setMax(100);
  cvPumpCap->setMin(30);
  cvPumpCap->setUnitOfMeasurement("%");
  // cvPumpCap->setDeviceClass("number");
  cvPumpCap->setStep(5);
  cvPumpCap->setMode(HANumber::ModeSlider);
  cvPumpCap->setRetain(false);
  cvPumpCap->setState(DEFAULT_CAP_PCT);

  cvPumpInputPwm->setName("Pump incoming pwm");
  cvPumpInputPwm->setIcon("mdi:gauge");
  cvPumpInputPwm->setUnitOfMeasurement("PWM");
  cvPumpInputPwm->setDeviceClass("power_factor");

  cvPumpOutputPwm->setName("Pump outgoing pwm");
  cvPumpOutputPwm->setIcon("mdi:gauge");
  cvPumpOutputPwm->setUnitOfMeasurement("PWM");
  cvPumpOutputPwm->setDeviceClass("power_factor");

  cvPumpInputPct->setName("Pump incoming");
  cvPumpInputPct->setIcon("mdi:gauge");
  cvPumpInputPct->setUnitOfMeasurement("%");
  cvPumpInputPct->setDeviceClass("power_factor");

  cvPumpOutputPct->setName("Pump outgoing");
  cvPumpOutputPct->setIcon("mdi:gauge");
  cvPumpOutputPct->setUnitOfMeasurement("%");
  cvPumpOutputPct->setDeviceClass("power_factor");

  // Additional relay
  if (AD_RELAY_ENABLED) {
    // handle command from the HA panel
    switch1->onCommand(onSwitchCommand);

    // Config.
    switch1->setName("Additional switch");
    switch1->setIcon("mdi:electric-switch");
    switch1->setState(false);
    Serial.println(F("Additional relay registered for HA"));
  }
}

void setupMqtt()
{
  Serial.print(F("Beginning MQTT. HA device version: "));
  Serial.println(HA_DEVICE_SOFTWARE_VERSION);
  mqtt->begin(BROKER_ADDR);
}

bool wifiConnect(int retries)
{
  int count = 0;
  String fv = WiFi.firmwareVersion();
  if (fv < WIFI_FIRMWARE_LATEST_VERSION)
  {
    Serial.println(F("Please upgrade the firmware"));
  }

  // attempt to connect to WiFi network:
  while (WiFi.status() != WL_CONNECTED && count < retries)
  {
    Serial.print(F("Attempting to connect to Network named: "));
    Serial.println(ssid); // print the network name (SSID);
    
    // int numSsid = WiFi.scanNetworks();
    // if (numSsid == -1) {
    //   Serial.println("No Wifi networks found");
    //   return false;
    // }
    // // print the network number and name for each network found:
    // bool found = false;
    // int netid_to_use = -1;
    // int best_rssi = -1;
    // for (int thisNet = 0; thisNet < numSsid; thisNet++) {
    //   if (strcmp(WiFi.SSID(thisNet), ssid)) {
    //     found = true;
    //     if (WiFi.RSSI(thisNet) > best_rssi) {
    //       best_rssi = WiFi.RSSI(thisNet);
    //       netid_to_use = thisNet;
    //     }
    //   }
    // }
    // if (!found) {
    //   Serial.println(F("No matching SSID found!"));
    //   return false;
    // }
    // Serial.println(F("Found proper AP, connecting"));
    // wifiStatus = WiFi.begin(ssid, pass, WiFi.BSSID(netid_to_use));

    // Current issue: WiFiNINA only connects to lowest channel.
    // Hence perhaps create a dedicated SSID one 1 AP just for the arduino.
    wifiStatus = WiFi.begin(ssid, pass);

    // wait 10 seconds for connection:
    delay(10000);
    count++;
  }
  printWifiStatus(); // you're connected now, so print out the status

  return (WiFi.status() == WL_CONNECTED);
}
void setup()
{
  Serial.begin(9600); // initialize serial communication
  // pinMode(9, OUTPUT);      // set the LED pin mode
  wifiConnect(20);
  setupHA();
  setupHASensors();
  setupMqtt();
  pwmSetup(pwm_in_pin, pwm_out_pin);

  if (AD_RELAY_ENABLED){
    relaySetup(relay_out_pin);
  }
}

bool checkConnection() {
  // If WiFi is down, try reconnecting.
    int status;
    bool _connectionOk = true;
    status = WiFi.status();
    if (status != WL_CONNECTED)
    {
      Serial.print(F("Wifi not connected: "));
      Serial.println(status);
      _connectionOk = false;
    }

    // Ping MQTT server, or reboot.
    if (_connectionOk && CONNECTION_CHECK_PING) {
      bool ping_ok = false;
      for (int i = 0; i < 5; i++) {
        pingResult = WiFi.ping(BROKER_ADDR);
        if (pingResult >= 0) {
          ping_ok = true;
          break;
        }
      }
      _connectionOk = ping_ok;
      if (!ping_ok) {
        Serial.println(F("Unable to ping MQTT broker. This will lead to reset."));
      }
    }
    return _connectionOk;
}

void fixConnection() {
    if (connectionProblemCount >= connectionProblemRebootTheshold) {
      Serial.println(F("Resetting device, too many connection failures so far"));
      resetFunc();
    }

    Serial.print(F("Reconnecting to WiFi. Problem count / threshold: "));
    Serial.print(connectionProblemCount);
    Serial.print(F("/"));
    Serial.println(connectionProblemRebootTheshold);
    // WiFi.disconnect();
    if (!wifiConnect(2)) {
      Serial.println(F("PROBLEM: Wifi unable to connect"));
      // Serial.println(F("Resetting device, too many Wifi connection failures"));
      // resetFunc();
    }
    setupMqtt();
}

void loop()
{
  currentMillis = millis();

  // Check connections.
  connectionProblem = false;
  if (currentMillis - wifiCheckpreviousMillis >= wifiCheckInterval) {
    wifiCheckpreviousMillis = currentMillis;
    connectionProblem = !checkConnection();
    if (connectionProblem) {
      connectionProblemCount++;
      fixConnection();
    } else {
      Serial.println(F("Connection ok"));
      connectionProblemCount = 0;
    }
  }

  // Update MQTT sensor, read incoming PWM. 
  int pwmIn = readPwmLoop();
  cvPumpInputPwm->setValue(pwmIn);
  int pwmInPct = round((float) pwmIn * 100 / 255);
  cvPumpInputPct->setValue(pwmInPct);

  // Update pwm.
  if (currentMillis - pwmSetPreviousMillis >= pwmSetInterval)
  {
    // Update pump output.
    pwmSetPreviousMillis = currentMillis;
    int cap = 100; // Safety, no cap.
    cap = (cvPumpCap->getCurrentState().isSet()) ? (int) cvPumpCap->getCurrentState().toInt32() : cap;

    if (cap < 15) {
      Serial.print(F("Cap under 15%! Should not be possible, correcting: "));
      Serial.println(cap);
    }

    int cap255 = round(((float) cap / 100) * 255);
    int pwmOut = capPwmOutput(cap255, pwmIn);
    cvPumpOutputPwm->setValue(pwmOut);
    int pwmOutPct = round((float) pwmOut * 100 / 255);
    cvPumpOutputPct->setValue(pwmOutPct);

    // Relay switch.
    switch1->setCurrentState(setRelay(switch1->getCurrentState()));
  }

  mqtt->loop();
}
