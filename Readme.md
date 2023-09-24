# About PiFence

An arduino script and a shell script for use as a fence agent on a custom built power supply for a five node raspberry pi cluster.

The shell script is a fence agent designed to be used with pacemaker in a HA environment. 
The Arduino program is for an Arduino with ethernet shield connected to five seperate relays that operate five USB power supplies. This setup allows the Arduino to function as a STONITH device for a five node Raspberry Pi cluster.

The whole project is pretty rough but it works well enough for "production" use. Plenty of room for improvement though.
Recently added the first revision of a custom PCB. All of the files are from KiCAD. I do not recommend using this PCB design as is because it is extremely simple and only uses various breakout boards. The next version will be a real PCB that includes all of the discrete components. 