# About PiFence PCB

The fence device is custom-made hardware that is run by the firmware in this repository. This PCB folder is the hardware.

The core of the hardware is an Arduino that controls five relays powering five USB ports. These ports provide power to each of five Raspberry Pi devices and the Arduino provides fencing capability. The Arduino has an ethernet port interface to provide external control.

The board has a power port for 18V input to supply the arduino and the five powered USB ports. 

Future plans include adding a USB C power input, two four-pin fan outputs, a 12V power output for the router, and integrating the ethernet port as well as converting the power outputs to USB C.

# Theory

The USB-C PD spec can deliver 20 volts and 5 amps which will provide up to 100 watts. This is plenty to power all of the board components. An alternate option is 15 volts at 3 amps. This will be just enough to power everything so long as there is not a high load on anything. Futher testing is probably required to ensure at least the initial bootup of all servers can be handled at the lower power level before providing support for it.

At the 20 volt input level power cannot be applied directly to the Arduino so a power step down will be required. A 12V step down will allow power to be applied to the arduino and to the router. A 5V step down with enough current output will be needed for the RPi power outputs and the fans.

Control of the fans should be provided by the Arduino. A temperature sensor will be added to the computer area of the case to provide input for controlling the speed of the fans.

# Component list

| Item          | Model              | Quantity |
| ------------- | ------------------ | -------- |
| MCU           | Arduino Nano Every | 1        |
| 12V Step down |                    | 1        |
| 5V Step down  |                    | ?        |
| Ethernet jack |                    | 1        |
| Relay         | PR13-5V-450-1C     | 5        |
| Temp sensor   |                    | 1        |
| USB C Port    |                    | 6        |

# Power requirements for all of the components

| Item    | Qty | Volt | Draw | Total  |
| ------- | --- | ---- | ---- | ------ |
| RPi4    | 5   | 5V   | 1.3A | 32.5W  |
| Fan     | 2   | 5V   | 0.2A | 2W     |
| Router  | 1   | 12V  | 1A   | 12W    |
| Arduino | 1   | 5V   | 25mA | 0.125W |
