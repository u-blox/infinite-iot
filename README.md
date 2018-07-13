# Introduction
This repository contains the code for the Technology group's Infinite IoT energy harvesting board.

# Fetching and Building
Import this code with:

`mbed import https://github.com/u-blox/infinite-iot`

Set your Mbed target to `UBLOX_EVK_NINA_B1`, set your desired toolchain and build the code with:

`mbed compile`

# Loading Code
To load a binary onto the Infinite IoT board you will need the following equipment:

1.  An adapter board, the one which forms part of the [u-blox B200 blueprint](https://www.u-blox.com/sites/default/files/Blueprint-B200_ApplicationNote_%28UBX-16009187%29.pdf); this plugs into the 10-way debug connector on the Infinite IoT board.
2.  An Olimex [ARM-JTAG-20-10](https://www.olimex.com/Products/ARM/JTAG/ARM-JTAG-20-10/) adapter board; the cable that comes with this board plugs into the header on the above adapter-board.
3.  A Segger [J-Link Base](https://www.segger.com/products/debug-probes/j-link/models/j-link-base/) debugger; the PCB header of the ARM-JTAG-20-10 adapter board plugs directly into this.

You will also need to install the Segger [JLink tools](https://www.segger.com/downloads/jlink/JLink_Windows.exe).

First, connect the Segger, Olimex cable and adapter board to the Infinite IoT board like this:

ADD PICTURE HERE

A binary can then be loaded using Segger's JLink Flash Lite.  Launch the JLink Flash Lite utility and chose `NRF52832_XXAA` as the device type; the interface will default to SWD and 4000 kHz, which is fine.  Select the `.hex` file you built above and then press Program Device.  A dialogue box should appear showing several bars of progress and then "done".  Reset the board and the binary will begin running.