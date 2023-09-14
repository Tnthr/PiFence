/*
 Pi Fence arduino sketch
 Used to manage a power supply for a Raspberry Pi Cluster
 Based on the Example shown below.
 Written by Brian Jenkins
 15 August 2022


 Udp NTP Client
 Get the time from a Network Time Protocol (NTP) time server
 Demonstrates use of UDP sendPacket and ReceivePacket
 For more on NTP time servers and the messages needed to communicate with them,
 see http://en.wikipedia.org/wiki/Network_Time_Protocol

 created 4 Sep 2010
 by Michael Margolis
 modified 9 Apr 2012
 by Tom Igoe
 modified 02 Sept 2015
 by Arturo Guadalupi

 This code is in the public domain.

 */

#include <Ethernet.h>
#include <EthernetUdp.h>
#include <SPI.h>

// #define DEBUG 1

#define POWEROFF HIGH
#define POWERON LOW

// Enter a MAC address for your controller below.
// Newer Ethernet shields have a MAC address printed on a sticker on the shield
//byte mac[] = {0xDE, 0xAD, 0xBE, 0xB0, 0x0B, 0x1E}; // For production with IP 10.0.1.77
byte mac[] = {0xDE, 0xAD, 0x02, 0xB0, 0x0B, 0x1E}; // For testing with IP 10.0.1.55


unsigned int localPort = 11089; // local port to listen for UDP packets
const int PACKET_SIZE = 48;     // Data packet is no more than 48 bytes
char packetBuffer[PACKET_SIZE]; // buffer to hold incoming and outgoing packets
char ReplyBuffer[PACKET_SIZE] = "ACK\n"; // a string to send back
int portPinStatus[6][3] = {
    {0, 0, 0}, // lazy filler
    {1, 2, 1}, // node 1 pin 2
    {2, 3, 1}, // node 2 pin 3
    // Skipped pin 4 because of some interference
    {3, 5, 1}, // node 3 pin 5
    {4, 6, 1}, // node 4 pin 6
    {5, 7, 1}  // node 5 pin 7
};
const int PIN = 1;
const int STATUS = 2;

// A UDP instance to let us send and receive packets over UDP
EthernetUDP Udp;

void setup() {
  // Set the relay pins to output and power on.
  for (int x = 1; x <= 5; x++) {
    pinMode(portPinStatus[x][1], OUTPUT);
    digitalWrite(portPinStatus[x][1], POWERON);
  }

#ifdef DEBUG
  // Open serial communications and wait for port to open:
  Serial.begin(9600);
  while (!Serial) {
    ; // wait for serial port to connect. Needed for native USB port only
  }

  Serial.println("Serial opened.");
#endif

  // start Ethernet and UDP
  if (Ethernet.begin(mac) == 0) {
#ifdef DEBUG
    Serial.println("Failed to configure Ethernet using DHCP");
    //  Check for Ethernet hardware present
    if (Ethernet.hardwareStatus() == EthernetNoHardware) {
      Serial.println(
          "Ethernet not found.  Sorry, can't run without hardware. :(");
    } else if (Ethernet.linkStatus() == LinkOFF) {
      Serial.println("Ethernet cable is not connected.");
    }
#endif
    // no point in carrying on, so do nothing forevermore:
    while (true) {
      delay(1);
    }
  } else {
#ifdef DEBUG
    if (Ethernet.linkStatus() == Unknown) {
      Serial.println("Link status unknown.");
    } else if (Ethernet.linkStatus() == LinkON) {
      Serial.println("Link status: On");
    } else if (Ethernet.linkStatus() == LinkOFF) {
      Serial.println("Link status: Off");
    }
#endif
  }
  Udp.begin(localPort);
#ifdef DEBUG
  Serial.println(Ethernet.localIP());
  Serial.println("Waiting for data...");
#endif
}

void loop() {
  // if there's data available, read a packet
  if (Udp.parsePacket()) {
    // read the packet into packetBuffer
    Udp.read(packetBuffer, PACKET_SIZE);

    // second char is the node number (1-5) which is converted to int
    int portNum = (packetBuffer[2] - '0');

    // zeroth character always R; first is what the request is
    if (packetBuffer[0] == 'R') { // This means its a request
      switch (packetBuffer[1]) {
      case 'S': // Status
#ifdef DEBUG
        Serial.println("Got an R and S");
#endif
        if (portPinStatus[portNum][STATUS] == 1) {
          strcpy(ReplyBuffer, "ON\n");
#ifdef DEBUG
          Serial.print("Port is ON: ");
          Serial.println(portNum);
#endif
        } else {
          strcpy(ReplyBuffer, "OFF\n");
#ifdef DEBUG
          Serial.print("Port is OFF: ");
          Serial.println(portNum);
#endif
        }
        break;
      case 'K': // Kill
#ifdef DEBUG
        Serial.println("Got an R and K");
#endif
        digitalWrite(portPinStatus[portNum][PIN], POWEROFF);
        portPinStatus[portNum][STATUS] = 0;
        strcpy(ReplyBuffer, "Success\n");
        break;
      case 'R': // Reboot
#ifdef DEBUG
        Serial.println("Got an R and R");
#endif
        digitalWrite(portPinStatus[portNum][PIN], POWEROFF);
        portPinStatus[portNum][STATUS] = 0;
        delay(2000); // 2 seconds delay should be enough
        digitalWrite(portPinStatus[portNum][PIN], POWERON);
        portPinStatus[portNum][STATUS] = 1;
        strcpy(ReplyBuffer, "Success\n");
        break;
      case 'P': // Power On
#ifdef DEBUG
        Serial.println("Got an R and P");
#endif
        digitalWrite(portPinStatus[portNum][PIN], POWERON);
        portPinStatus[portNum][STATUS] = 1;
        strcpy(ReplyBuffer, "Success\n");
        break;
      }
    }

    // send a reply, to the IP address and port that sent us the packet
    Udp.beginPacket(Udp.remoteIP(), Udp.remotePort());
    Udp.write(ReplyBuffer);
    Udp.endPacket();
  }

  Ethernet.maintain();
}