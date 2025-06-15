/*
This program runs on a Raspberry Pi Pico connected via USB to the Elumatech saw computer. Another Pico is connected via USB to the barcode receiver, and translates the HID commands into plaintext which it spits out over UART serial at 115200 baud. The Pico running this program receives and parses the data.
Updated 26 February 2025
*/

#include <MouseAbsolute.h>
#include "Keyboard.h"

#define START_X 31000
#define START_Y 28500
#define STOP_X 27800
#define STOP_Y 28500

#define DOOR_OFFSET -493
#define SHORTCUT_OFFSET 176

// example message: A3210.9X6Z
// 3210.9mm and case 6
const byte numChars = 10;  // I think the max is 10
char receivedChars[numChars];
char tempChars[numChars];  // temporary array for use when parsing
// Serial parsing functions taken from example 5 at this link:
// https://forum.arduino.cc/t/serial-input-basics-updated/382007/3
// variables to hold the parsed data
float floatLength = 0.0;
int integerCase = 0;
bool newData = false;

float length = 0.0;
int angleL = 45;
int angleR = 45;
int angleCase = 0;
bool dontpressstart = 0;

#define DEBUG_MODE 0

void setup() {
  Serial1.begin(115200);  // barcode scanner port... pin 2 (GPIO 1) is RX
  // idea: try waiting a sec for the other pi to start, then read serial to clear buffer
  delay(2000);
  Serial1.read(); // get rid of data... might just be a hello message
  // while(Serial1.available() && millis() < 5000) {
  //   Serial1.read(); // get rid of data... might just be a hello message
  // }
  if (DEBUG_MODE) { Serial.begin(115200); }
  Keyboard.begin();
  MouseAbsolute.begin();  // 211,158 screen w,h in mm
}
// seems to work, only bug since new parse method is the first scan doesn't seem to work
void loop() {
  recvWithStartEndMarkers();
  if (newData == true) {
    strcpy(tempChars, receivedChars);
    // this temporary copy is necessary to protect the original data
    //   because strtok() used in parseData() replaces the dividing character with \0
    parseData();
    length = floatLength;
    angleCase = integerCase;
    if (DEBUG_MODE) {
      Serial.print(length);
      Serial.print(", ");
      Serial.println(angleCase);
    }
    else {
      moveSaw(length, angleCase);  // now, do >400mm check in this function
    }
    newData = false;
  }
}

void parseData() {  // split the data into its parts

  char* strtokIndx;  // this is used by strtok() as an index

  strtokIndx = strtok(tempChars, "X");  // get the first part - the float
  floatLength = atof(strtokIndx);       // convert this part to a float

  strtokIndx = strtok(NULL, "X");  // this continues where the previous call left off
  integerCase = atoi(strtokIndx);  // convert this part to an integer
}
void recvWithStartEndMarkers() {
  static boolean recvInProgress = false;
  static byte ndx = 0;
  char startMarker = 'A';
  char endMarker = 'Z';
  char rc;

  while (Serial1.available() > 0 && newData == false) {
    rc = Serial1.read();
    
    if (recvInProgress == true) {
      if (rc != endMarker) {
        receivedChars[ndx] = rc;
        ndx++;
        if (ndx >= numChars) {
          ndx = numChars - 1;
        }
      } else {
        receivedChars[ndx] = '\0';  // terminate the string
        recvInProgress = false;
        ndx = 0;
        newData = true;
      }
    }

    else if (rc == startMarker) {
      recvInProgress = true;
    }
  }
}

// picked slightly different variable names than global ones
void moveSaw(float leng, int angCase) {
  switch (angCase) {
    case 0:  // normal (/\)
      angleL = 45;
      angleR = 45;
      dontpressstart = 0;
      break;
    case 1:  // trapezoid left (/|) + 25mm
      angleL = 45;
      angleR = 90;
      dontpressstart = 0;
      leng = leng + 25;
      break;
    case 2:  // trapezoid right (|\) + 25mm
      angleL = 90;
      angleR = 45;
      dontpressstart = 0;
      leng = leng + 25;
      break;
    case 3:  // trapezoid top (||) + 50mm
      angleL = 90;
      angleR = 90;
      dontpressstart = 0;
      leng = leng + 50;
      break;
    case 4:  // mullion, astragal (||)
      angleL = 90;
      angleR = 90;
      dontpressstart = 0;
      break;
    case 5:  // prerouted door sticks
      angleL = 45;
      angleR = 45;
      dontpressstart = 1;  // don't press start to encourage user to notice and turn off right blade
      leng = leng + DOOR_OFFSET;
      break;
    case 6:  // shortcut (/\) + SHORTCUT_OFFSET
      angleL = 45;
      angleR = 45;
      dontpressstart = 1;  // don't press start to encourage user to notice and add spacer
      leng = leng + SHORTCUT_OFFSET;
      break;
    default:
      angleL = 45;
      angleR = 45;
      dontpressstart = 0;
      break;
  }
  if (leng > 400) {  // only move if it's a valid length
    pressStop();
    delay(1500);
    Keyboard.print(leng);
    Keyboard.write(KEY_RETURN);
    Keyboard.write(KEY_TAB);
    Keyboard.print(angleL);
    Keyboard.write(KEY_TAB);
    Keyboard.print(angleR);
    delay(200);
    if (!dontpressstart) {
      pressStart();  // usually do this, unless it's a door (case 5) or shortcut (case 6)
    }
  }
}

void pressStart() {
  MouseAbsolute.move(START_X, START_Y);
  delay(10);
  MouseAbsolute.click();
}
void pressStop() {
  MouseAbsolute.move(STOP_X, STOP_Y);
  delay(10);
  MouseAbsolute.click();
}
