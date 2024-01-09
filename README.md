# Intergas circulation pump cap / limiter.

>> This was just a Friday night project that allows me to sleep better.  
You might as well find code improvements; just bear with me and let me know :)

This Arduino project was developed for the Daikin Altherma `NHY2KOMB-A / EJHA-AV3` bundle, based on the `Arduino Uno Wifi Rev2` board.

This is a Daikin `EJHA-AV3` hybrid air/water heatpump, bundled with an Intergas `NHY2KOMB` gas heater.

Then again, it's mostly a generic PWM manipulation project, useable for a ton of other use cases.

## Background
Usually any gas heater has a circulation pump limitation feature, which is used by the plumber in case there is over capacity, causing audible noise in the pipes and radiators.

In the case of this bundle, the heatpump's control system is fully controlling the heater.  
In some scenarios, the configured limit is completely ignored, thanks to Daikin's engineers.

They may or may not have good reasons for this, but fact is that   
 - it's norm to have the capability of limiting circulation flow since decades
 - Intergas has a nice pump with huge capacity, and thanks to the original software, this can be finetuned and limited. No problem if this indeed works.

Except that the Daikin unit ignores it, in case of
 - gas involved heating (only gas heater, or hybrid)
 - frost prevention (typically causing annoying noise during the night when outside temperatures drop).

After begin on the phone with Daikin a few times, there does not seem to be a solution to this from their end.

## How it works

### Heater and pump
The gas heater has a circulation pump, in my case a `Wilo Yonos PARA RS15/7.5 PWM1 M`.  
This pump is controlled with `PWM` (pulse width modulation), similar to fan speed control.

The Wilo datasheet explains the signal used, what `HIGH` voltage it needs, and what PWM frequency it supports.

Also note that the pump is configured to use the PWM signal reversed, meaning that a low duty cycle means full speed, a high duty cycle means low speed.
This is (most probably) a safety measure, so that when there's a connection problem (no signal), the pump will always run at full speed.

### Arduino
This Arduino project simply measures the duty cycle from the heater interface (`in`), applies a limit / cap to it, and reproduces another PWM signal based on that (`out`).

Note that the Wilo pump mentioned above needs `5v` for the `HIGH` signal, meaning that the typical `3v` arduino boards don suffice.

For this reason it's adviced to use the `Arduino Uno Wifi Rev2` board (operates at `5v`).

# Wiring

> **Use at your own risk; consult the installation manual of your heater and check where the pump's signal wire is going!**

For the `NHY2KOMB-A` heater, the incoming PWM signal is on bus `#5` on the `X4` connector row.  

>> _Note the numbering starts at 4 (see sticker), so bus #5 is the _second_ socket. CHECK THIS YOURSELF._  
To verify this, just follow the signal wire (the one stickered with `No 230V!`) from the pump.  
`Black` means `ground`, `red` means `PWM signal`.

 - The heater PWM bus needs to be connected to the Arduino, in the example settings (check your `include/arduino_secrets.h`) on pin `10`.
 - Pin `5` (see example configs again) needs to be connected to the signal wire of the pump, the red wire you just disconnected from the heater bus.
 - The Arduino must be grounded to the ground strip of the heater. The `black` wire from the signal cable is also grounded here.

# Setup
This project makes the following assumptions:
 - MQTT up and running on some IP address
 - Home assistant with MQTT integration up and running (just to control the cap and see in/out PWM signals)
 - Wifi with WPA/WPA2
 - An `Arduino Uno Wifi Rev2` board
 - You're not an idiot that will connect this board to some `230V` bus on the furnice :)

Steps:
  - Setup Visual Code with/or Platform IO
  - Clone this project
  - Copy `include/arduino_secrets.example.h` into `include/arduino_secrets.h`, change the wifi, MQTT IP (and optionally pin)values
  - Connect the 3 wires properly
  - Build and upload the project to your Arduino board
  - make sure the heater casing is closed (safety)

After this,
 - the Arduino should connect to your wifi
 - the Arduino should connect and report to MQTT
 - Home assistant should report a new device `Heater Arduino Uno` with
   - PWM in/out, both 8 bit values and percentage
   - a cap number input slider, allowing you to set it from HA

## Additional relay
Could for example be used in a scenario where
  - there's an opentherm thermostat
  - there's a woodstove burning, hence thermostat switches to no heating
  - the rest of your house gets cold
 In this case the relay might be connected to the on/off terminal of your furnice/boiler.
 Check if the boiler is capable of running both opentherm and on/off at the same time.

To realise, set `AD_RELAY_ENABLED` to `true` and `RELAY_OUT_PIN` to another free digital pin,
like pin nr 2.
Since a relay board usually also needs both a 5v and a ground pin, don't forget to connect those.

# Additional hints
 - Make an automation with HA or Node Red to use a lower cap at night.
 - In case you have [ESPAltherma](https://raomin.github.io/ESPAltherma/), use the max cap in case the temperature delta (input/output temp of your heater/heatpump) is over 10°C .
 - Without Wifi connection, the project will (currently) not proceed to operation and will now work. To use it stand alone, just remove the Wifi and MQTT start code from the `setup()` method and add an additional cap value parameter somewhere set to your liking.

# Possible future changes and improvements (please make a PR)
 - Configure MQTT credentials
   See https://dawidchyrzynski.github.io/arduino-home-assistant/documents/library/mqtt-security.html 
 - Configure MQTT TLS
 - Make the PWM stickiness configurable in HA
