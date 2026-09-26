// CU40025SCPB-U5A "Hello World" - the minimum needed to get text on screen.
//
// Wiring (8-bit bus, M68 mode):
//   Arduino pin : 2  3   4  5   6   7   8   9   10  11  12
//   Display pin : 4  5   6  7   8   9   10  11  12  13  14
//   Signal      : RS R/W E  DB0 DB1 DB2 DB3 DB4 DB5 DB6 DB7
//
// Power (split supply, both needed):
//   Display pin 1 = GND,  pin 2  = +5 V   (logic)
//   Display pin 17 = GND, pin 18 = +18 V minimum (VFD drive)
// Tie both grounds to the Arduino GND.
//
// No R/W wire? Tie display pin 5 to GND and pass CU40025VFD::NO_PIN
// instead of 3 below. Everything except reading back still works.

#include <CU40025VFD.h>

//             RS R/W E  DB0 DB1 DB2 DB3 DB4 DB5 DB6 DB7
CU40025VFD vfd(2, 3,  4, 5,  6,  7,  8,  9,  10, 11, 12);

void setup() {
  vfd.begin();
  vfd.print("Hello, world!");
  vfd.setCursor(0, 1);
  vfd.print("CU40025SCPB-U5A 2x40 VFD");
}

void loop() {
  // Seconds counter at the right-hand end of line 1
  vfd.setCursor(30, 0);
  vfd.print(millis() / 1000);
  delay(250);
}
