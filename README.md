# Introduction
This repository contains the code for the Technology group's Energy Harvesting board.

More info to go here.

# Testing
This repo is provided with unit tests.  To run them, you first need to [once] do:

`mbedls --m 0004:UBLOX_EVK_NINA_B1`

This will ensure that Mbed sees a `UBLOX_EVK_NINA_B` target, otherwise it will see an `LPC2368` target.  You can then run the unit tests with:

`mbed test -ntests-unit_tests-default`

If you set `"mbed-trace.enable": true` in the `target_overrides` section of `mbed_app.json` and add `-v` to your `mbed test` command line, you will get debug prints from the test execution.

If you need to debug a test, you will need to build everything with debug symbols on:

`mbed test -ntests-unit_tests-default --profile=mbed-os/tools/profiles/debug.json`

...and then, to run the tests, go and find the built binary (down in `BUILD\tests\UBLOX_EVK_NINA_B1\GCC_ARM\TESTS...`) and drag/drop it onto the Mbed mapped drive presented by the board. To run the test under a debugger, run `mbedls` to determine the COM port that your board is connected to. Supposing it is COM1, you would then run the target board under your debugger and, on the PC side, enter the following to begin the tests:

`mbedhtrun --skip-flashing --skip-reset -p COM1:9600`