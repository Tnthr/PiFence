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

#include <SPI.h>
#include <Ethernet.h>
#include <EthernetUdp.h>

// Enter a MAC address for your controller below.
// Newer Ethernet shields have a MAC address printed on a sticker on the shield
byte mac[] = {
  0xDE, 0xAD, 0xBE, 0xB0, 0x0B, 0x1E
};

unsigned int localPort = 11089;       // local port to listen for UDP packets
const int PACKET_SIZE = 48; // Data packet is no more than 48 bytes
char packetBuffer[PACKET_SIZE]; //buffer to hold incoming and outgoing packets
char ReplyBuffer[PACKET_SIZE] = "ACK\n";  // a string to send back

// A UDP instance to let us send and receive packets over UDP
EthernetUDP Udp;

void setup() {
  // Set the relay pins to output and low
  pinMode(13, OUTPUT);
  digitalWrite(13, LOW);
  pinMode(12, OUTPUT);
  digitalWrite(12, LOW);
  pinMode(11, OUTPUT);
  digitalWrite(11, LOW);
  pinMode(10, OUTPUT);
  digitalWrite(10, LOW);
  pinMode(9, OUTPUT);
  digitalWrite(9, LOW);

  // Open serial communications and wait for port to open:
  Serial.begin(9600);
  while (!Serial) {
    ; // wait for serial port to connect. Needed for native USB port only
  }

  // start Ethernet and UDP
  if (Ethernet.begin(mac) == 0) {
    Serial.println("Failed to configure Ethernet using DHCP");
    // Check for Ethernet hardware present
    if (Ethernet.hardwareStatus() == EthernetNoHardware) {
      Serial.println("Ethernet shield was not found.  Sorry, can't run without hardware. :(");
    } else if (Ethernet.linkStatus() == LinkOFF) {
      Serial.println("Ethernet cable is not connected.");
    }
    // no point in carrying on, so do nothing forevermore:
    while (true) {
      delay(1);
    }
  }
  Udp.begin(localPort);

  Serial.println("Waiting for data...");
}

void loop() {
  // if there's data available, read a packet
  if (Udp.parsePacket()) {
    // read the packet into packetBuffer
    Udp.read(packetBuffer, PACKET_SIZE);

    int stat = 0;

    //if (len > 0) {
    //  packetBuffer[len] = "0";
    //}

    // first character always R; second is what the request is
    if (packetBuffer[0] == 'R') { // This means its a request, should never be anything else
      switch (packetBuffer[1]) {
        case 'S': // Status
          Serial.println("Got an R and S");
          stat = digitalRead(packetBuffer[2] + 8);
          if (stat == LOW) {
            strcpy(ReplyBuffer, "ON\n");
          } else {
            strcpy(ReplyBuffer, "OFF\n");
          }
          break;
        case 'K': // Kill
          Serial.println("Got an R and K");
          digitalWrite(packetBuffer[2], LOW);
          strcpy(ReplyBuffer, "Success\n");
          break;
        case 'R': // Reboot
          Serial.println("Got an R and R");
          digitalWrite(packetBuffer[2], LOW);
          delay(2000); // 2 seconds delay should be enough
          digitalWrite(packetBuffer[2], HIGH);
          strcpy(ReplyBuffer, "Success\n");
          break;
        case 'P': // Power On
          Serial.println("Got an R and P");
          digitalWrite(packetBuffer[2], HIGH);
          strcpy(ReplyBuffer, "Success\n");
          break;
      }
    }

    // send a reply, to the IP address and port that sent us the packet we received
    Udp.beginPacket(Udp.remoteIP(), Udp.remotePort());
    Udp.write(ReplyBuffer);
    Udp.endPacket();
  }

  //delay(100);
  Ethernet.maintain();
}

// send a UDP packet for a response
void sendPacket(const char * address) {
  // set the second half of the bytes in the buffer to 0
  memset(packetBuffer, 0, PACKET_SIZE);
  // Initialize values needed to form NTP request
  // (see URL above for details on the packets)
  packetBuffer[0] = 0b11100011;   // LI, Version, Mode
  packetBuffer[1] = 0;     // Stratum, or type of clock
  packetBuffer[2] = 6;     // Polling Interval
  packetBuffer[3] = 0xEC;  // Peer Clock Precision
  // 8 bytes of zero for Root Delay & Root Dispersion
  packetBuffer[12]  = 49;
  packetBuffer[13]  = 0x4E;
  packetBuffer[14]  = 49;
  packetBuffer[15]  = 52;

  // all NTP fields have been given values, now
  // you can send a packet requesting a timestamp:
  Udp.beginPacket(address, 123); // NTP requests are to port 123
  Udp.write(packetBuffer, PACKET_SIZE);
  Udp.endPacket();
}