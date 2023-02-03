# IR simulator
A tool to control devices without the need for an IR remote. It simulates an IR receiver using the Linux gpiod and has been tested on the Nextthing CHIP. It should also work on the Raspberry Pi.

# Build
To build the project, run the following command:
`sudo make`

Note that root access is required for setuid, allowing it to be run by any user. The IR simulator also uses real-time scheduling to ensure accurate IR timing.
