// Copy this file into `arduino_secrets.h`.

// Wifi credentials, assuming WPA/WPA2.
// Note that WiFiNINA has a shortcorming in picking the AP with the lowest channel number
// rather than best signal, in case of multiple AP's, often resulting in poor connection.
// Workaround is to have a dedicated SSID on a closeby AP.
// @see https://github.com/arduino-libraries/WiFiNINA/issues/200 .
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

// Additional relay.
// Could for example be used in a scenario where
//  - there's an opentherm thermostat
//  - there's a woodstove burning, hence thermostat switches to no heating
//  - the rest of your house gets cold
// In this case the relay might be connected to the on/off terminal of your furnice/boiler.
// Check if the boiler is capable of running both opentherm and on/off at the same time.
const bool AD_RELAY_ENABLED = true;
const byte RELAY_OUT_PIN = 2;

// Connection ping check.
// This will enable a ping-check against the MQTT server, alongside the regular wifi connection check.
// After 5 connection issue instances, the board will reset itself.
const bool CONNECTION_CHECK_PING = true;