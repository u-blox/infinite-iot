# Outline
In outline, this is how the energy harvesting code is structured:

- There are `eh_` modules and `act_` modules: `eh_` modules form the main structure while `act_` modules are actions that can be taken when enough energy is available.  The modules are all written in C, though they may instantiate C++ entities.
- At power-on `eh_post` performs a power-on self test, calling all of the `act_` modules in turn to check them out and, while checking out the `act_modem` module, it also determines what kind of modem is attached (SARA-R410 or SARA-N211).
- With this done, `eh_processor` is called.  `eh_processor` first checks that there is enough energy to continue; if there is not it returns immediately.  If there is sufficient energy to continue it looks at the power sources available to it and determines which one should be used for the next period.  Then it looks through the history of previous actions and uses that to determine the optimal order of actions to take next.  When it has made a list of actions it executes the corresponding `act_h` modules as rapidly as possible in parallel tasks and then returns, hopefully without running out of energy.
- When `eh_processor` returns the system is put to sleep, RAM retained, until either an RTC timer expires or motion is detected, at which point `eh_processor` is called again, etc.
- Each `act_` module may produce output in the form of a data structure which is held in RAM. The `eh_data` module stores these data structures in a sorted list. The `act_modem` module is the only thing that can empty the list, sending the data off to a server on the internet. The possible actions are:
    - `act_voltage`: measure an analogue voltage,
    - `act_cellular`: measure cellular parameters,
    - `act_temperature_humidity_pressure`: measure the environment, implemented by `act_bme280`,
    - `act_light`: measure light levels, implemented by `act_si1133`,
    - `act_magnetic`: measure magnetic field strength, implemented by `act_si7210`,
    - `act_orientation`: measure orientation, implemented by `act_lis3dh`,
    - `act_position`: measure position, implemented by `act_zoem8`,
    - `act_modem`: make a report to the server or get the internet time,
    - `ble`: search for BLE devices, implemented by the NINA-B1 module itself.
- There are also data structures which may be added to the list without an associated action, such as statistics on device operation, logging, etc.
- The data structures are coded in JSON by the `eh_codec` module for sending to the server.
- The single config file, `eh_config.h`, specifies what pins are used for what, I2C addresses of the sensors, modem details (APN etc.).

# Debugging
There is only one UART on the NINA-B1 module, which is normally connected to the modem. During development, the modem can be left switched off and an FTDI cable soldered to pads on the Energy Harvesting board to allow printf() debug.  To obtain printf() debug, add the following to `mbed_app.json`:

```
    "config": {
        "enable_printf": true
    }
```

Otherwise, when the modem is required, local debug is via one single colour LED.  The module `eh_morse` provides a Morse code LED flash for last resort debug.

Another option is to connect a debugger and use the `SWO` pin to obtain `printf()`s.  To redirect prints to `SWO`, add the following entry to `mbed_app.json`:

```
{
    "target_overrides": {
        "UBLOX_EVK_NINA_B1": {
            "target.macros_add": ["ENABLE_PRINTF_SWO"]
        }
     }
}
```

`SWO` prints can then be viewed in several PC applications.  The instructions [here](https://mcuoneclipse.com/2016/10/17/tutorial-using-single-wire-output-swo-with-arm-cortex-m-and-eclipse/) show how to set them up, though I only managed to get the Eclipse instructions to work and then only with odd gaps between each printed letter.  Anyhoo, better than nothing.  The `SWO` frequency of the NRF52832 target processor used in NINA-B1 is 4 MHz and the target clock frequency 64 MHz.

During normal operation, logging is written to data structures by the `log-client` library and these data structures are transmitted to the server, along with everything else, where they can be decoded and examined. Note, however, that this is necessarily very low bandwidth (and tightly packed) logging.
