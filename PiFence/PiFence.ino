/*
 Pi Fence arduino sketch
 Used to manage a power supply and fans for a Raspberry Pi Cluster
 Attempted to include fault tolerance so it will not fail
 Written by Brian Jenkins
 15 August 2022
*/

#include <SPI.h>
#include <Ethernet.h>
#include <EthernetUdp.h>
#include <OneWire.h>
#include <DallasTemperature.h>

// Setup for debugging output and to disable unneeded things (serial connection) for production
const int DEBUG = 1;

// The MAC address for the controller.
byte mac[] = {
  0xDE, 0xAD, 0xBE, 0xB0, 0x0B, 0x1E
};

// Data wire is plugged into digital pin 4 on the Arduino
#define ONE_WIRE_BUS 4

// Setup a oneWire instance to communicate with any OneWire device
OneWire oneWire(ONE_WIRE_BUS);  

// Pass oneWire reference to DallasTemperature library
DallasTemperature sensors(&oneWire);

unsigned int localPort = 11089;           // local port to listen for UDP packets
const int PACKET_SIZE = 48;               // Data packet is no more than 48 bytes
char packetBuffer[PACKET_SIZE];           // buffer to hold incoming and outgoing packets
char ReplyBuffer[PACKET_SIZE] = "ACK\n";  // a string to send back
int portPinStatus[6][3] = {
                          {0, 0, 0},  // lazy filler
                          {1, 2, 1},  // node 1, pin 2, status on
                          {2, 3, 1},  // node 2, pin 3, status on
                          {3, 5, 1},  // node 3, pin 5, status on
                          {4, 6, 1},  // node 4, pin 6, status on
                          {5, 7, 1}   // node 5, pin 7, status on
};
const int PIN = 1;
const int STATUS = 2;
int EthernetStatus = 0;

// Variables for the fan control
const byte OC1A_PIN = 9;
const byte OC1B_PIN = 10;
const word PWM_FREQ_HZ = 25000; //Adjust this value to adjust the frequency
const word TCNT1_TOP = 16000000/(2*PWM_FREQ_HZ);
const int SETTEMPINTERVAL = 3000;

// A UDP instance to let us send and receive packets over UDP
EthernetUDP Udp;

void setup() {
  // Set the relay pins to output and low. In this case 'low' is powered on
  for (int x = 1; x <= 5; x++) {
    pinMode(portPinStatus[x][1], OUTPUT);
    digitalWrite(portPinStatus[x][1], LOW);
  }

  // Control pin for fan
  pinMode(OC1B_PIN, OUTPUT);

  // PWM settings
  // Clear Timer1 control and count registers
  TCCR1A = 0;
  TCCR1B = 0;
  TCNT1  = 0;

  // Set Timer1 configuration
  // COM1A(1:0) = 0b10   (Output A clear rising/set falling)
  // COM1B(1:0) = 0b00   (Output B normal operation)
  // WGM(13:10) = 0b1010 (Phase correct PWM)
  // ICNC1      = 0b0    (Input capture noise canceler disabled)
  // ICES1      = 0b0    (Input capture edge select disabled)
  // CS(12:10)  = 0b001  (Input clock select = clock/1)
  
  TCCR1A |= (1 << COM1A1) | (1 << WGM11);
  TCCR1B |= (1 << WGM13) | (1 << CS10);
  ICR1 = TCNT1_TOP;

  if (DEBUG) {
    // Open serial communications and wait for port to open:
    Serial.begin(9600);
    while (!Serial) {
      ; // wait for serial port to connect. Needed for native USB port only
    }

    Serial.println("Serial opened.");
  }

  // Start up the temperature library 
  sensors.begin();
  if (DEBUG) {
    Serial.println("Temp sensor started.");
  }

  // start Ethernet and UDP
  if (Ethernet.begin(mac) == 0) {
    if (DEBUG) {
      Serial.println("Failed to configure Ethernet using DHCP");
    }
    // Check for Ethernet hardware present
    if (Ethernet.hardwareStatus() == EthernetNoHardware) {
      if (DEBUG) {
        Serial.println("Ethernet shield was not found.");
      }
    } else if (Ethernet.linkStatus() == LinkOFF) {
      if (DEBUG) {
        Serial.println("Ethernet cable is not connected.");
      }
    }
    if (DEBUG) {
      Serial.println("Ethernet failed. But we will continue to run the fans.");
    }
  } else {
    EthernetStatus = Udp.begin(localPort);
    if (DEBUG) {
      Serial.println("Ethernet started. Waiting for data...");
    }
  }
}

void loop() {
  // read the temp and set the fan speed
  setFanSpeed();

  // if there's data available, read a packet
  if (EthernetStatus) {
    if (Udp.parsePacket()) {
      // read the packet into packetBuffer
      Udp.read(packetBuffer, PACKET_SIZE);

      // The port received will be 1-5 which is cast to int
      int portNum = (packetBuffer[2] - '0');

      // first character always R; second is what the request is
      if (packetBuffer[0] == 'R') { // This means its a request, should never be anything else
        switch (packetBuffer[1]) {
          case 'S': // Status
            if (DEBUG) {
              Serial.println("Got an R and S");
            }
            if (portPinStatus[portNum][STATUS] == 1) {
              strcpy(ReplyBuffer, "ON\n");

              if (DEBUG) {
                Serial.print("Port is ON: ");
                Serial.println(portNum);
              }
            } else {
              strcpy(ReplyBuffer, "OFF\n");

              if (DEBUG) {
                Serial.print("Port is OFF: ");
                Serial.println(portNum);
              }
            }
            break;
          case 'K': // Kill
            if (DEBUG) {
              Serial.println("Got an R and K");
            }
            digitalWrite(portPinStatus[portNum][PIN], HIGH);
            portPinStatus[portNum][STATUS] = 0;
            strcpy(ReplyBuffer, "Success\n");
            break;
          case 'R': // Reboot
            if (DEBUG) {
              Serial.println("Got an R and R");
            }
            digitalWrite(portPinStatus[portNum][PIN], HIGH);
            portPinStatus[portNum][STATUS] = 0;
            delay(2000); // 2 seconds delay should be enough
            digitalWrite(portPinStatus[portNum][PIN], LOW);
            portPinStatus[portNum][STATUS] = 1;
            strcpy(ReplyBuffer, "Success\n");
            break;
          case 'P': // Power On
            if (DEBUG) {
              Serial.println("Got an R and P");
            }
            digitalWrite(portPinStatus[portNum][PIN], LOW);
            portPinStatus[portNum][STATUS] = 1;
            strcpy(ReplyBuffer, "Success\n");
            break;
        }
      }

      // send a reply, to the IP address and port that sent us the packet we received
      Udp.beginPacket(Udp.remoteIP(), Udp.remotePort());
      Udp.write(ReplyBuffer);
      Udp.endPacket();
    }
  }

  if (DEBUG) {
    Serial.println("delaying...");
    delay(1000);
  }
  Ethernet.maintain();  // keep the ethernet connection alive
}

// set the fan PWM
void setPwmDuty(byte duty) {
  OCR1A = (word) (duty*TCNT1_TOP)/100;
  if (DEBUG) {
    Serial.print(" / Fan duty cycle: ");
    Serial.println(duty);
  }
}

// Adjust the fan PWM based on the temperature reading
void setFanSpeed() {
  sensors.requestTemperatures();
  int temp = sensors.getTempCByIndex(0);

  if (DEBUG) {
    Serial.print("Temp via sensor: ");
    Serial.print(temp);
  }
  
  switch (temp) {
    case 20:
    case 21:
    case 22:
    case 23:
    case 24:
    case 25:
      setPwmDuty(25);
      break;
    case 26:
      setPwmDuty(50);
      break;
    case 27:
      setPwmDuty(75);
      break;
    case 28:
      setPwmDuty(100);
      break;
    default:
      setPwmDuty(100);
  }
}