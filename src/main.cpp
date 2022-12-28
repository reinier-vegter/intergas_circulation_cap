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

#include "arduino_secrets.h"
///////please enter your sensitive data in the Secret tab/arduino_secrets.h
char ssid[] = SECRET_SSID; // your network SSID (name)
char pass[] = SECRET_PASS; // your network password (use for WPA, or use as key for WEP)
int default_cap_pct = DEFAULT_CAP_PCT;
int pwm_in_pin = PWM_IN_PIN;
int pwm_out_pin = PWM_OUT_PIN;
char ha_device_name[] = HA_DEVICE_NAME;
IPAddress ip;
unsigned long currentMillis = millis();

int wifiStatus = WL_IDLE_STATUS;
WiFiClient client;
unsigned long wifiCheckInterval = 20000;
unsigned long wifiCheckpreviousMillis = 0;

unsigned long pwmSetInterval = 3000;
unsigned long pwmSetPreviousMillis = 0;

byte mac[] = {0x00, 0x10, 0xFA, 0x6E, 0x38, 0x4A};
HADevice device(mac, sizeof(mac));
HAMqtt mqtt(client, device);

// Cap prct
HANumber cvPumpCap("cvCirculationPumpCap");
HASensorNumber cvPumpInputPwm("cvPumpInput");
HASensorNumber cvPumpOutputPwm("cvPumpOutput");
HASensorNumber cvPumpInputPct("cvPumpInputPct");
HASensorNumber cvPumpOutputPct("cvPumpOutputPct");
int currentPumpCap = 100;

void printWifiStatus()
{
  // print the SSID of the network you're attached to:
  Serial.print(F("SSID: "));
  Serial.println(WiFi.SSID());

  // print your board's IP address:
  ip = WiFi.localIP();
  Serial.print(F("IP Address: "));
  Serial.println(ip);

  // print the received signal strength:
  long rssi = WiFi.RSSI();
  Serial.print(F("signal strength (RSSI):"));
  Serial.print(rssi);
  Serial.println(" dBm");
}

void onNumberCommand(HANumeric number, HANumber *sender)
{
  Serial.print(F("Cap updated from MQTT: "));
  Serial.println(number.getBaseValue());
  sender->setState(number); // report the selected option back to the HA panel
}

void setupMqtt()
{
  // set device's details (optional)
  device.setName(ha_device_name);
  device.setSoftwareVersion("1.0.2");
  device.enableSharedAvailability();
  device.enableLastWill();

  // handle command from the HA panel
  cvPumpCap.onCommand(onNumberCommand);

  // Optional configuration
  cvPumpCap.setIcon("mdi:gauge");
  cvPumpCap.setName("CV Circulation pump cap");
  cvPumpCap.setMax(100);
  cvPumpCap.setMin(30);
  cvPumpCap.setUnitOfMeasurement("%");
  // cvPumpCap.setDeviceClass("number");
  cvPumpCap.setStep(5);
  cvPumpCap.setMode(HANumber::ModeSlider);
  cvPumpCap.setRetain(false);
  cvPumpCap.setState(DEFAULT_CAP_PCT);

  cvPumpInputPwm.setName("Pump incoming pwm");
  cvPumpInputPwm.setIcon("mdi:gauge");
  cvPumpInputPwm.setUnitOfMeasurement("PWM");
  cvPumpInputPwm.setDeviceClass("power_factor");

  cvPumpOutputPwm.setName("Pump outgoing pwm");
  cvPumpOutputPwm.setIcon("mdi:gauge");
  cvPumpOutputPwm.setUnitOfMeasurement("PWM");
  cvPumpOutputPwm.setDeviceClass("power_factor");

  cvPumpInputPct.setName("Pump incoming");
  cvPumpInputPct.setIcon("mdi:gauge");
  cvPumpInputPct.setUnitOfMeasurement("%");
  cvPumpInputPct.setDeviceClass("power_factor");

  cvPumpOutputPct.setName("Pump outgoing");
  cvPumpOutputPct.setIcon("mdi:gauge");
  cvPumpOutputPct.setUnitOfMeasurement("%");
  cvPumpOutputPct.setDeviceClass("power_factor");
  mqtt.begin(BROKER_ADDR);
}

void wifiConnect()
{
  String fv = WiFi.firmwareVersion();
  if (fv < WIFI_FIRMWARE_LATEST_VERSION)
  {
    Serial.println("Please upgrade the firmware");
  }

  // attempt to connect to WiFi network:
  while (wifiStatus != WL_CONNECTED)
  {
    Serial.print(F("Attempting to connect to Network named: "));
    Serial.println(ssid); // print the network name (SSID);

    // Connect to WPA/WPA2 network. Change this line if using open or WEP network:
    wifiStatus = WiFi.begin(ssid, pass);
    // wait 10 seconds for connection:
    delay(10000);
  }
  printWifiStatus(); // you're connected now, so print out the status
}
void setup()
{
  Serial.begin(9600); // initialize serial communication
  // pinMode(9, OUTPUT);      // set the LED pin mode
  wifiConnect();
  setupMqtt();
  pwmSetup(pwm_in_pin, pwm_out_pin);
}

void loop()
{
  // if WiFi is down, try reconnecting
  currentMillis = millis();

  int status;
  status = WiFi.status();
  if ((status == WL_DISCONNECTED || status == WL_CONNECTION_LOST) && (currentMillis - wifiCheckpreviousMillis >= wifiCheckInterval))
  {
    Serial.println("Reconnecting to WiFi...");
    WiFi.disconnect();
    wifiCheckpreviousMillis = currentMillis;
    wifiConnect();
    setupMqtt();
  }

  // Update MQTT sensor, read incoming PWM. 
  int pwmIn = readPwmLoop();
  cvPumpInputPwm.setValue(pwmIn);
  int pwmInPct = round((float) pwmIn * 100 / 255);
  cvPumpInputPct.setValue(pwmInPct);

  // Update pump output.
  if (currentMillis - pwmSetPreviousMillis >= pwmSetInterval)
  {
    pwmSetPreviousMillis = currentMillis;
    int cap = 100; // Safety, no cap.
    cap = (cvPumpCap.getCurrentState().isSet()) ? (int) cvPumpCap.getCurrentState().toInt32() : cap;

    if (cap < 15) {
      Serial.print(F("Cap under 15%! Should not be possible, correcting: "));
      Serial.println(cap);
    }

    int cap255 = round(((float) cap / 100) * 255);
    int pwmOut = capPwmOutput(cap255, pwmIn);
    cvPumpOutputPwm.setValue(pwmOut);
    int pwmOutPct = round((float) pwmOut * 100 / 255);
    cvPumpOutputPct.setValue(pwmOutPct);
  }

  mqtt.loop();
}
