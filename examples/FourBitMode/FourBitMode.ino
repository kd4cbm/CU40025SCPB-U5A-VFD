// CU40025SCPB-U5A in 4-bit mode: saves 4 I/O pins.
//
// Only DB4-DB7 are used; leave display pins 7-10 (DB0-DB3) unconnected.
// Each byte is sent as two nibbles, high nibble first.
//
// Wiring:
//   Arduino pin : 2  3   4  9   10  11  12
//   Display pin : 4  5   6  11  12  13  14
//   Signal      : RS R/W E  DB4 DB5 DB6 DB7
//
// Power: pin 1 GND, pin 2 +5 V, pin 17 GND, pin 18 +18 V minimum.
//
// With the 8-bit wiring from the other examples, this sketch also works
// as-is: DB0-DB3 are simply ignored.

#include <CU40025VFD.h>

//             RS R/W E  DB4 DB5 DB6 DB7
CU40025VFD vfd(2, 3,  4, 9,  10, 11, 12);

void setup() {
  Serial.begin(115200);
  bool readable = vfd.begin();
  Serial.print(F("4-bit mode, read-back "));
  Serial.println(readable ? F("working") : F("not available"));

  vfd.print(F("4-bit mode: DB4-DB7 only"));
  vfd.setCursor(0, 1);
  vfd.print(F("Read-back: "));
  vfd.print(readable ? F("OK") : F("n/a"));

  // Verify a character read back through the 4-bit bus
  if (readable) {
    uint8_t c = vfd.readChar(0, 0);
    vfd.setCursor(20, 1);
    vfd.print(F("char(0,0)='"));
    vfd.write(c);
    vfd.print(F("' 0x"));
    vfd.print(c, HEX);
  }
}

void loop() {
  // Cycle brightness so the brightness instruction is exercised in 4-bit mode too
  static uint8_t level = 0;
  vfd.setBrightness((CU40025VFD::Brightness)level);
  vfd.setCursor(15, 1);
  vfd.print(100 - level * 25);
  vfd.print(F("% "));
  level = (level + 1) & 3;
  delay(1500);
}
