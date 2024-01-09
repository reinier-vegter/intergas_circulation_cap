#include <Arduino.h>

byte _RELAY_OUT_PIN;

/**
 * Set relay state. True = on, false = off.
 */
bool setRelay(bool state)
{
  digitalWrite(_RELAY_OUT_PIN, (state ? HIGH : LOW));
  Serial.print(F("Relay set to: "));
  Serial.println(state ? "HIGH" : "LOW");
  return state;
}

void relaySetup(byte pin_output)
{
    _RELAY_OUT_PIN = pin_output;
    pinMode(_RELAY_OUT_PIN, OUTPUT);
    Serial.print(F("Relay setup default. "));
    setRelay(false);
}
