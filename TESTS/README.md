# Testing
This repo is provided with unit tests which run in the `mbed test` environment.  They will run on several different mbed-enabled boards (see below) though unfortunately not on the Infinite IoT energy harvesting board itself as it does not include a spare serial port, required to talk to the PC-side test infrastructure.

# Platforms
The tests run on the following platforms.

|  Test         |  Platform       |  Notes |
|:-------------:|:---------------:|--------|
| `action`    | `UBLOX_C030_U201`, `TB_SENSE_12` | |
| `data`      | `UBLOX_C030_U201`, `TB_SENSE_12` | |
| `codec`     | `UBLOX_C030_U201`, `TB_SENSE_12` | |
| `processor` | `UBLOX_C030_U201`, `TB_SENSE_12` | |
| `modem` | `UBLOX_C030_U201` | To run the test that sends reports to a server you will need to run the Python script that is stored in the modem test directory on a machine which is visible to the public internet and make sure that the `SERVER_ADDRESS` and `SERVER_PORT` #defines point to that same machine.|
| `si1133`    | `TB_SENSE_12` | |
| `si7210`    | `TB_SENSE_12` | |
| `lis3dh`    | `UBLOX_C030_U201` | Need to attach an external LIS3DH eval board (e.g. STEVAL-MKI105V1, in which case tie CS high to get an I2C interface and SD0 low to get the right I2C address) with I2C wired to the I2C pins on the Arduino header.|
| `bme280`    | `UBLOX_C030_U201` | Need to attach an external BME280 eval board (e.g. MIKROE-1978) with I2C wired to the I2C pins on the Arduino header.|
| `zoem8`     | `UBLOX_C030_U201` | Need to attach a u-blox GNSS board (the tests aren't specific to the u-blox ZOE part, so something like a u-blox PAM-7Q board would be fine) to the I2C pins on the Arduino header.|
| `multi_i2c`| `UBLOX_C030_U201` | Need to attach all three of the above (i.e. an LIS3DH eval board, a BME280 eval board and a u-blox GNSS board) to the I2C pins on the Arduino header and make sure that the I2C pins in `eh_config.h` are correct for that board (probably by defining the right pin numbers in `mbed_app.json`.)|

In addition, any tests which are not marked as `TB_SENSE_12` _only_ will also run on a standard `UBLOX_EVK_NINA_B1`, though note that the usage of pins on the NINA-B1 EVK is different to that on the Infinite IoT board and hence some jiggery-pokery with pin definitions will be required.  And of course, with some small modifications, the `si1133` and `si7210` unit tests will run on a `UBLOX_C030_U201` or `UBLOX_EVK_NINA_B1` board if an evaluation board carrying a `si1133` or `si7210` is attached to the I2C pins of those boards.

# Building The Tests
In order not to get tangled up in the operation of the energy harvesting hardware, when running any test _apart_ from the `multi_i2c` integration test, you also need to ensure that your `mbed_app.json` includes the following:

```
    "config": {
        "disable_peripheral_hw": true
    }
```

This will spew out warnings about unused functions, which can be ignored.

# Running The Tests
Run the unit tests with:

`mbed test -ntests-unit_tests*`

If you set `"mbed-trace.enable": true` in the `target_overrides` section of `mbed_app.json` and add `-v` to your `mbed test` command line, you will get debug prints from the test execution.

If you need to debug a test, you will need to build everything with debug symbols on:

`mbed test -ntests-unit_tests* --profile=mbed-os/tools/profiles/debug.json`

...and then go and find the built binary (down in something like `BUILD\tests\UBLOX_C030_U201\GCC_ARM\TESTS...`) and program the board with that binary. To run the test, determine the COM port that your board is connected to and then, supposing it is COM1, start the target board under your debugger and, on the PC side, enter the following to begin the tests:

`mbedhtrun --skip-flashing --skip-reset -p COM1:9600`
