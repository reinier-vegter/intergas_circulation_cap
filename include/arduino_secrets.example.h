// Copy this file into `arduino_secrets.h`.

// Wifi credentials, assuming WPA/WPA2.
#define SECRET_SSID ""
#define SECRET_PASS ""

// IP of MQTT broker.
#define BROKER_ADDR IPAddress(192,168,1,20)

// Name of this device in Home Assistant.
#define HA_DEVICE_NAME "Heater Arduino Uno";

// Pin for incoming PWM signal from heater.
const byte PWM_IN_PIN = 10;

// Pin for outgoing PWM signal to pump.
const byte PWM_OUT_PIN = 5;

// Default circulation cap (%) in case of no instruction.
const byte DEFAULT_CAP_PCT = 60;
