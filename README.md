# Introduction
This repository contains the code for the Technology group's Energy Harvesting board.

# Fetching and Building
Import this code with:

`mbed import https://github.com/u-blox/tec_eh`

Set your Mbed target to `UBLOX_EVK_NINA_B1`, set your desired toolchain and build the code with:

`mbed compile`

Connect a programming board to the Energy Harvesting board and drag/drop the compiled binary onto the USB driver that appears in order to program the NINA-B1 module.

# SW Description
The NINA-B1 module is the control processor of the Energy Harvesting board.  It spends most of its time asleep, consuming just 1.2 uA.  It wakes up either periodically or when motion or a magnetic field is detected.  When awoken it checks if there is enough energy to operate.  If there is, it examines its action history to determine the most efficient set of actions to perform.  Actions include reading from the pressure, humidity, temperature, UV, light, orientation and GNSS sensors and sending any readings off to the cellular network using the Sara cellular module.  Once done it returns to sleep.