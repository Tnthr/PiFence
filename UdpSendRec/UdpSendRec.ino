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

// #include <Ethernet.h>
#include <EthernetUdp.h>
#include <SPI.h>

#define DEBUG 1

// MAC address for the controller
byte mac[] = {0xDE, 0xAD, 0x02, 0xB0, 0x0B, 0x1E};

unsigned int localPort = 11089;          // local port to listen for UDP packets
const int PACKET_SIZE = 16;              // Data packet is no more than 16 bytes
char packetBuffer[PACKET_SIZE];          // buffer to hold incoming packets
char ReplyBuffer[PACKET_SIZE] = "ACK\n"; // a string to send back
int portPinStatus[6][3] = {
    {0, 0, 0}, // lazy filler
    {1, 2, 1}, // node 1, pin 2, status 1
    {2, 3, 1}, // node 2, pin 3, status 1
    {3, 5, 1}, // node 3, pin 5, status 1
    {4, 6, 1}, // node 4, pin 6, status 1
    {5, 7, 1}  // node 5, pin 7, status 1
};
const int PIN = 1;
const int STATUS = 2;

// A UDP instance to let us send and receive packets over UDP
EthernetUDP Udp;

void setup() {
  // Set the relay pins to output and low. In this case 'low' is powered on
  for (int x = 1; x <= 5; x++) {
    pinMode(portPinStatus[x][PIN], OUTPUT);
    digitalWrite(portPinStatus[x][PIN], LOW);
  }

// Open serial communications and wait for port to open
#ifdef DEBUG
  Serial.begin(9600);
  while (!Serial) {
    ; // wait for serial port to connect. Needed for native USB port only
  }

  Serial.println("Serial opened.");
#endif
  // start Ethernet and UDP
  while (Ethernet.begin(mac) == 0) {
#ifdef DEBUG
    Serial.println("Failed to configure Ethernet using DHCP");
#endif
    //  Check for Ethernet hardware present
    if (Ethernet.hardwareStatus() == EthernetNoHardware) {
#ifdef DEBUG
      Serial.println("Ethernet shield was not found.  Sorry, can't run "
                     "without hardware. :(");
#endif
    } else if (Ethernet.linkStatus() == LinkOFF) {
#ifdef DEBUG
      Serial.println("Ethernet cable is not connected.");
#endif
    }
    delay(10000);
#ifdef DEBUG
    Serial.println("Trying ethernet connection again.");
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

    // The port received will be 1-5 which is cast to int
    int portNum = (packetBuffer[2] - '0');

    if (portNum > 5 || portNum < 1) {
#ifdef DEBUG
      Serial.println("Bad port number.");
#endif
    }

    // first character always R; second is what the request is
    if (packetBuffer[0] == 'R') { // This means its a request
      switch (packetBuffer[1]) {
      case 'S': // Status
#ifdef DEBUG
        Serial.print("Got an R and S. ");
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
        Serial.print("Got an R and K. port: ");
        Serial.println(portNum);
#endif
        digitalWrite(portPinStatus[portNum][PIN], HIGH);
        portPinStatus[portNum][STATUS] = 0;
        strcpy(ReplyBuffer, "Success\n");
        break;
      case 'R': // Reboot
#ifdef DEBUG
        Serial.print("Got an R and R. port: ");
        Serial.println(portNum);
#endif
        digitalWrite(portPinStatus[portNum][PIN], HIGH);
        portPinStatus[portNum][STATUS] = 0;
        delay(1000); // 1 second delay should be enough
        digitalWrite(portPinStatus[portNum][PIN], LOW);
        portPinStatus[portNum][STATUS] = 1;
        strcpy(ReplyBuffer, "Success\n");
        break;
      case 'P': // Power On
#ifdef DEBUG
        Serial.print("Got an R and P. port: ");
        Serial.println(portNum);
#endif
        digitalWrite(portPinStatus[portNum][PIN], LOW);
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

  // delay(100);
  Ethernet.maintain();
}