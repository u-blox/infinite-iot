# Testing
This repo is provided with unit tests.  To run them, you first need to [once] do:

`mbedls --m 0004:UBLOX_EVK_NINA_B1`

This will ensure that Mbed sees a `UBLOX_EVK_NINA_B1` target, otherwise it will see an `LPC2368` target.

In order not to get tangled up in the operation of the energy harvesting hardware, you also need to ensure that your `mbed_app.json` includes the following:

```
    "config": {
        "disable_peripheral_hw": true
    },
```

You can then run the unit tests with:

`mbed test -ntests-unit_tests*`

If you set `"mbed-trace.enable": true` in the `target_overrides` section of `mbed_app.json` and add `-v` to your `mbed test` command line, you will get debug prints from the test execution.

If you need to debug a test, you will need to build everything with debug symbols on:

`mbed test -ntests-unit_tests* --profile=mbed-os/tools/profiles/debug.json`

...and then, to run the tests, go and find the built binary (down in `BUILD\tests\UBLOX_EVK_NINA_B1\GCC_ARM\TESTS...`) and drag/drop it onto the Mbed mapped drive presented by the board. To run the test under a debugger, run `mbedls` to determine the COM port that your board is connected to. Supposing it is COM1, you would then run the target board under your debugger and, on the PC side, enter the following to begin the tests:

`mbedhtrun --skip-flashing --skip-reset -p COM1:9600`

# Testing On Other Platforms
As well as testing on the energy harvesting board, it is also possible to run some or all of these unit tests on other platforms.  The table below details what is possible.

|  Test         |  Platform       |  Notes |
|:-------------:|:---------------:|--------|
| `action`    | `UBLOX_C030_U201`, `TB_SENSE_12` | |
| `data`      | `UBLOX_C030_U201`, `TB_SENSE_12` | Since these board have more RAM and a test fills RAM, run time is longer. |
| `codec`     | `UBLOX_C030_U201`, `TB_SENSE_12` | |
| `processor` | `UBLOX_C030_U201`, `TB_SENSE_12` | |
| `si1133`    | `TB_SENSE_12`     | |
| `si7210`    | `TB_SENSE_12`     | |
| `lis3dh`    | `UBLOX_C030_U201` | Need to attach an external LIS3DH eval board with I2C wired to the I2C pins on the Arduino header.|
| `bme280`    | `UBLOX_C030_U201` | Need to attach an external BME280 eval board with I2C wired to the I2C pins on the Arduino header.|

In addition, the tests which are not marked as `TB_SENSE_12` only will also run on a standard `UBLOX_EVK_NINA_B1`.  And of course, with some small modifications, the `si1133` and `si7210` will run on a `UBLOX_C030_U201` or `UBLOX_EVK_NINA_B1` board if an evaluation board carrying a `si1133` or `si7210` is attached to the I2C pins of those boards.