#include <Arduino.h>
#include <math.h>

byte _PWM_IN_PIN;
byte _PWM_OUT_PIN;
int pwm_value;
const int PWM_TIMEOUT_MICROSECONDS = 10000;
const int PWM_255_SNAP_DIFFERENCE = 4; // Dont change outgoing PWM unless it differs more than this from the last set value.
int pwm255OldValue = 0;
unsigned long pwmCheckInterval = 3000;
unsigned long pwmCheckpreviousMillis = 0;
unsigned long currentPwmMillis = millis();

void pwmSetup(byte pin_input, byte pin_output)
{
  _PWM_IN_PIN = pin_input;
  pinMode(_PWM_IN_PIN, INPUT);
  _PWM_OUT_PIN = pin_output;
  pinMode(_PWM_OUT_PIN, OUTPUT);
}

/*
Reads incoming PWM, 0 ~ 255.
0 stopping the pump, 255 full speed (the inverse of a Wilo pump).
*/
int readPwmSignal()
{
  unsigned long highDuration = pulseIn(_PWM_IN_PIN, HIGH, PWM_TIMEOUT_MICROSECONDS);
  unsigned long lowDuration = pulseIn(_PWM_IN_PIN, LOW, PWM_TIMEOUT_MICROSECONDS);
  // Serial.print(F("High / Low duration: "));
  // Serial.print(highDuration);
  // Serial.print(F(" / "));
  // Serial.println(lowDuration);

  if (highDuration == 0 || lowDuration == 0)
  {
    return digitalRead(_PWM_IN_PIN) == HIGH ? 255 : 0;
  }

  float r = 255 - ((float)(highDuration * 255) / (highDuration + lowDuration));
  return round(r);
}

int readPwmAverage()
{
  float averageReading = 0;
  int MeasurementsToAverage = 20;
  for (int i = 0; i < MeasurementsToAverage; ++i)
  {
    averageReading += readPwmSignal();
    delay(10);
  }
  averageReading /= MeasurementsToAverage;
  return round(averageReading);
}

int readPwmLoop()
{
  currentPwmMillis = millis();
  if (currentPwmMillis - pwmCheckpreviousMillis >= pwmCheckInterval)
  {
    pwmCheckpreviousMillis = currentPwmMillis;
    pwm_value = readPwmAverage();
    Serial.print(F("PWM incoming (0-255): "));
    Serial.println(pwm_value);
  }
  return pwm_value;
}

/**
 * Set PWM speed, 0 slow, 255 full.
 */
int setPwmOut(int value)
{
  int inverted = 255 - value;
  int pwmVal = inverted;

  // Outer boundaries.
  pwmVal = (pwmVal > 255) ? 255 : pwmVal;
  pwmVal = (pwmVal < 0) ? 0 : pwmVal;

  // Snap to current value.
  if (abs(pwmVal - pwm255OldValue) > PWM_255_SNAP_DIFFERENCE)
  {
    pwm255OldValue = pwmVal;
    analogWrite(_PWM_OUT_PIN, pwmVal);
    Serial.print(F("Setting PWM (0-255) to "));
    Serial.println(pwmVal);
  }
  return 255 - pwm255OldValue;
}

/**
 * Cap the current pwmIn to pwmMax (both 0 ~ 255);
 */
int capPwmOutput(int pwmMax, int pwmIn)
{
  if (pwmIn > pwmMax)
  {
    Serial.print(F("Max exceeded! Capping to (0-255) "));
    Serial.println(pwmMax);
    return setPwmOut(pwmMax);
  }
  else
  {
    return setPwmOut(pwmIn);
  }
}
