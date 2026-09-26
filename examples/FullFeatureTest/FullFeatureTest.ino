// CU40025SCPB-U5A full feature test, using the CU40025VFD library.
//
// Exercises every feature in the Noritake datasheet (DS-805-0000-02), one
// labelled test at a time, and loops forever. Progress is also logged on
// Serial at 115200 baud.
//
// Wiring (8-bit, M68 mode, R/W connected so read-back can be tested):
//   Arduino pin : 2  3   4  5   6   7   8   9   10  11  12
//   Display     : RS R/W E  DB0 DB1 DB2 DB3 DB4 DB5 DB6 DB7
// Power: pin 1 GND, pin 2 +5 V, pin 17 GND, pin 18 +18 V (minimum).
//
// Test 5 lights every dot, which is the display's maximum current draw.

#include <CU40025VFD.h>

CU40025VFD vfd(2, 3, 4, 5, 6, 7, 8, 9, 10, 11, 12);

const uint8_t COLS = CU40025VFD::COLS;
const uint8_t NUM_TESTS = 10;

// ---------------------------------------------------------------------------
// User font data: rows[0..6] = 5x7 dots, rows[7] = underline
// ---------------------------------------------------------------------------
const uint8_t SLOT_FULL = 5, SLOT_CHECK_A = 6, SLOT_CHECK_B = 7;
const uint8_t baseGlyphs[8][8] PROGMEM = {
  { 0x10, 0x10, 0x10, 0x10, 0x10, 0x10, 0x10, 0x00 },  // 0-4: bar 1..5 columns wide
  { 0x18, 0x18, 0x18, 0x18, 0x18, 0x18, 0x18, 0x00 },
  { 0x1C, 0x1C, 0x1C, 0x1C, 0x1C, 0x1C, 0x1C, 0x00 },
  { 0x1E, 0x1E, 0x1E, 0x1E, 0x1E, 0x1E, 0x1E, 0x00 },
  { 0x1F, 0x1F, 0x1F, 0x1F, 0x1F, 0x1F, 0x1F, 0x00 },
  { 0x1F, 0x1F, 0x1F, 0x1F, 0x1F, 0x1F, 0x1F, 0x1F },  // 5: all 36 dots on
  { 0x15, 0x0A, 0x15, 0x0A, 0x15, 0x0A, 0x15, 0x00 },  // 6: checker
  { 0x0A, 0x15, 0x0A, 0x15, 0x0A, 0x15, 0x0A, 0x1F },  // 7: inverse checker
};

const uint8_t iconGlyphs[8][8] PROGMEM = {
  { 0x00, 0x0A, 0x0A, 0x00, 0x11, 0x0E, 0x00, 0x00 },  // smiley
  { 0x00, 0x0A, 0x1F, 0x1F, 0x0E, 0x04, 0x00, 0x00 },  // heart
  { 0x04, 0x0E, 0x0E, 0x0E, 0x1F, 0x00, 0x04, 0x00 },  // bell
  { 0x02, 0x03, 0x02, 0x02, 0x0E, 0x1E, 0x0C, 0x00 },  // note
  { 0x0E, 0x11, 0x11, 0x1F, 0x1B, 0x1B, 0x1F, 0x00 },  // padlock
  { 0x18, 0x18, 0x07, 0x08, 0x08, 0x08, 0x07, 0x00 },  // degrees C
  { 0x04, 0x0E, 0x15, 0x04, 0x04, 0x04, 0x04, 0x00 },  // up arrow
  { 0x0E, 0x11, 0x11, 0x1F, 0x11, 0x11, 0x11, 0x1F },  // 'A' with underline (dot 36)
};

const uint8_t pacOpen[8] PROGMEM   = { 0x0E, 0x1B, 0x1E, 0x1C, 0x1E, 0x1F, 0x0E, 0x00 };
const uint8_t pacClosed[8] PROGMEM = { 0x0E, 0x1B, 0x1F, 0x1F, 0x1F, 0x1F, 0x0E, 0x00 };

void loadGlyphs(const uint8_t set[8][8]) {
  for (uint8_t i = 0; i < 8; i++) vfd.createChar_P(i, set[i]);
}

// ---------------------------------------------------------------------------
// Helpers
// ---------------------------------------------------------------------------
uint16_t loopCount = 0;
bool lastReadbackPass = false;

void header(uint8_t n, const __FlashStringHelper *name, const __FlashStringHelper *detail) {
  vfd.setDisplayControl(true, false, false);
  vfd.clear();
  vfd.print(F("Test "));
  vfd.print(n);
  vfd.print('/');
  vfd.print(NUM_TESTS);
  vfd.print(F(": "));
  vfd.print(name);
  vfd.setCursor(0, 1);
  vfd.print(detail);

  Serial.print(F("\n[Test "));
  Serial.print(n);
  Serial.print(F("] "));
  Serial.print(name);
  Serial.print(F(" - "));
  Serial.println(detail);
  delay(2200);
}

void printPadded(uint8_t row, const __FlashStringHelper *s) {
  vfd.setCursor(0, row);
  uint8_t n = vfd.print(s);
  while (n++ < COLS) vfd.write(' ');
}

void fillLine(uint8_t row, uint8_t c) {
  vfd.setCursor(0, row);
  for (uint8_t i = 0; i < COLS; i++) vfd.write(c);
}

uint8_t pattern(uint8_t i) { return 0x21 + (i * 7) % 94; }

// ---------------------------------------------------------------------------
// Tests
// ---------------------------------------------------------------------------
void splash() {
  vfd.clear();
  vfd.print(F("   Noritake Itron CU40025SCPB-U5A VFD"));
  vfd.setCursor(0, 1);
  vfd.print(F("  2x40 5x7+UL  8-bit M68  Feature Test"));
  Serial.print(F("\n=== CU40025SCPB-U5A feature test, loop "));
  Serial.println(loopCount);
  delay(3000);
}

// Busy flag, address counter, DD RAM and CG RAM read-back
void testReadback() {
  header(1, F("Bus read-back"), F("BF/AC read, DD RAM + CG RAM verify"));

  if (!vfd.canRead()) {
    printPadded(0, F("Read-back FAILED: busy flag/AC unreadable"));
    printPadded(1, F("Check R/W wiring. Using fixed delays."));
    Serial.println(F("  Busy flag probe failed - reads not possible"));
    lastReadbackPass = false;
    delay(4000);
    return;
  }

  // Fill all 80 cells with a known pattern (visible on screen)
  for (uint8_t row = 0; row < 2; row++) {
    vfd.setCursor(0, row);
    for (uint8_t col = 0; col < COLS; col++) vfd.write(pattern(row * COLS + col));
  }
  delay(1500);

  // Sequential DD RAM read (address auto-increments on read)
  uint8_t ddOk = 0;
  for (uint8_t row = 0; row < 2; row++) {
    vfd.setCursor(0, row);
    for (uint8_t col = 0; col < COLS; col++) {
      uint8_t expect = pattern(row * COLS + col);
      uint8_t got = vfd.readData();
      if (got == expect) ddOk++;
      else {
        Serial.print(F("  DD RAM mismatch r"));
        Serial.print(row);
        Serial.print(F(" c"));
        Serial.print(col);
        Serial.print(F(" exp 0x"));
        Serial.print(expect, HEX);
        Serial.print(F(" got 0x"));
        Serial.println(got, HEX);
      }
    }
  }

  // Address counter: line 2, col 17 = 0x51
  vfd.setCursor(17, 1);
  uint8_t ac = vfd.readAddress();
  bool acOk = (ac == 0x51);

  // CG RAM: define all 8 slots with a pattern, read every row back
  for (uint8_t slot = 0; slot < 8; slot++) {
    uint8_t rows[8];
    for (uint8_t r = 0; r < 8; r++) rows[r] = ((slot * 8 + r) * 5 + 3) & 0x1F;
    rows[7] = slot & 1;  // underline on odd slots
    vfd.createChar(slot, rows);
  }
  uint8_t cgOk = 0;
  for (uint8_t slot = 0; slot < 8; slot++) {
    for (uint8_t r = 0; r < 8; r++) {
      uint8_t expect = (r < 7) ? (((slot * 8 + r) * 5 + 3) & 0x1F) : ((slot & 1) ? 0x04 : 0);
      uint8_t mask = (r < 7) ? 0x1F : 0x04;  // row 8: only dot 36 (DB2) is defined
      uint8_t got = vfd.readCharRow(slot, r);
      if ((got & mask) == expect) cgOk++;
      else {
        Serial.print(F("  CG RAM mismatch slot "));
        Serial.print(slot);
        Serial.print(F(" row "));
        Serial.print(r);
        Serial.print(F(" exp 0x"));
        Serial.print(expect, HEX);
        Serial.print(F(" got 0x"));
        Serial.println(got, HEX);
      }
    }
  }
  loadGlyphs(baseGlyphs);

  lastReadbackPass = (ddOk == 80) && acOk && (cgOk == 64);

  vfd.clear();
  vfd.print(F("DD RAM r/w "));
  vfd.print(ddOk == 80 ? F("PASS") : F("FAIL"));
  vfd.print(F(" ("));
  vfd.print(ddOk);
  vfd.print(F("/80)  AC read "));
  vfd.print(acOk ? F("PASS") : F("FAIL"));
  vfd.setCursor(0, 1);
  vfd.print(F("CG RAM r/w "));
  vfd.print(cgOk == 64 ? F("PASS") : F("FAIL"));
  vfd.print(F(" ("));
  vfd.print(cgOk);
  vfd.print(F("/64)  Busy flag OK"));

  Serial.print(F("  DD RAM ")); Serial.print(ddOk); Serial.println(F("/80"));
  Serial.print(F("  AC read 0x")); Serial.print(ac, HEX); Serial.println(acOk ? F(" PASS") : F(" FAIL"));
  Serial.print(F("  CG RAM ")); Serial.print(cgOk); Serial.println(F("/64"));
  delay(4000);
}

// Every DD RAM address on both lines
void testAddressing() {
  header(2, F("DD RAM addressing"), F("80 cells: 00-27H line 1, 40-67H line 2"));

  vfd.clear();
  for (uint8_t c = 0; c < COLS; c++) vfd.write('0' + c % 10);
  vfd.setCursor(0, 1);
  for (uint8_t c = 0; c < COLS; c++) vfd.write('0' + c / 10);
  Serial.println(F("  Column ruler (units on line 1, tens on line 2)"));
  delay(3000);

  // Walk a solid block through every cell
  vfd.clear();
  for (uint8_t row = 0; row < 2; row++) {
    for (uint8_t col = 0; col < COLS; col++) {
      vfd.setCursor(col, row);
      vfd.write(SLOT_FULL);
      delay(30);
      vfd.setCursor(col, row);
      vfd.write(' ');
    }
  }

  // Fill line 1 left-to-right, line 2 right-to-left
  for (uint8_t col = 0; col < COLS; col++) {
    vfd.setCursor(col, 0);
    vfd.write(SLOT_FULL);
    delay(15);
  }
  for (int8_t col = COLS - 1; col >= 0; col--) {
    vfd.setCursor(col, 1);
    vfd.write(SLOT_FULL);
    delay(15);
  }
  delay(800);
}

// All 256 character codes, 64 per page
void testCharset() {
  header(3, F("Character font"), F("All 256 codes, 00-0F = user CG RAM"));

  for (uint8_t page = 0; page < 4; page++) {
    vfd.clear();
    for (uint8_t row = 0; row < 2; row++) {
      vfd.setCursor(0, row);
      for (uint8_t half = 0; half < 2; half++) {
        uint8_t hi = page * 4 + row * 2 + half;
        if (half) vfd.write(' ');
        vfd.print(hi, HEX);
        vfd.print(F("x:"));
        for (uint8_t lo = 0; lo < 16; lo++) vfd.write((uint8_t)(hi * 16 + lo));
      }
    }
    Serial.print(F("  Codes 0x"));
    Serial.print(page * 64, HEX);
    Serial.print(F("-0x"));
    Serial.println(page * 64 + 63, HEX);
    delay(4500);
  }
}

// User definable fonts, underline dot, live CG RAM update, bar graph
void testUserFonts() {
  header(4, F("User fonts (CG RAM)"), F("8 chars, 5x7 + underline dot 36"));

  loadGlyphs(iconGlyphs);
  vfd.setCursor(0, 0);
  vfd.print(F("Codes 00-07: "));
  for (uint8_t i = 0; i < 8; i++) { vfd.write(i); vfd.write(' '); }
  vfd.print(F(" <-last=UL"));
  vfd.setCursor(0, 1);
  vfd.print(F("Codes 08-0F: "));
  for (uint8_t i = 8; i < 16; i++) { vfd.write(i); vfd.write(' '); }
  vfd.print(F(" (mirror)"));
  Serial.println(F("  Icons: smiley heart bell note lock degC arrow A+underline"));
  delay(5000);

  // Rewriting CG RAM changes characters already on screen
  printPadded(0, F("Live CG RAM rewrite (one slot animated)"));
  fillLine(1, '.');
  for (uint8_t col = 0; col < COLS; col++) {
    vfd.createChar_P(7, (col & 1) ? pacClosed : pacOpen);
    vfd.setCursor(col, 1);
    vfd.write(7);
    if (col) {
      vfd.setCursor(col - 1, 1);
      vfd.write(' ');
    }
    delay(120);
  }
  delay(500);

  // Bar graph built from 5 partial-width glyphs
  loadGlyphs(baseGlyphs);
  printPadded(0, F("Bar graph, 5 steps/cell:"));
  fillLine(1, ' ');
  for (uint16_t p = 0; p <= COLS * 5; p++) {
    vfd.setCursor(25, 0);
    uint8_t pct = p * 100UL / (COLS * 5);
    if (pct < 100) vfd.write(' ');
    if (pct < 10) vfd.write(' ');
    vfd.print(pct);
    vfd.write('%');
    uint8_t full = p / 5, part = p % 5;
    vfd.setCursor(full ? full - 1 : 0, 1);
    if (full) vfd.write(4);
    if (part && full < COLS) vfd.write(part - 1);
    delay(15);
  }
  delay(1500);
}

// All dots on and checker patterns (software equivalent of the self-test)
void testAllDots() {
  header(5, F("All dots / checker"), F("Every dot incl. underline (max current)"));

  fillLine(0, SLOT_FULL);
  fillLine(1, SLOT_FULL);
  Serial.println(F("  All 80 x 36 dots on"));
  delay(3000);

  for (uint8_t i = 0; i < 8; i++) {
    uint8_t a = (i & 1) ? SLOT_CHECK_B : SLOT_CHECK_A;
    uint8_t b = (i & 1) ? SLOT_CHECK_A : SLOT_CHECK_B;
    for (uint8_t row = 0; row < 2; row++) {
      vfd.setCursor(0, row);
      for (uint8_t col = 0; col < COLS; col++) vfd.write(((col + row) & 1) ? b : a);
    }
    delay(600);
  }
  Serial.println(F("  Checker / inverse checker alternated"));
}

// Brightness 100 / 75 / 50 / 25 %
void testBrightness() {
  header(6, F("Brightness"), F("Function Set + data byte BR1:BR0"));

  static const char *const labels[4] = { "00 = 100%", "01 =  75%", "10 =  50%", "11 =  25%" };
  vfd.clear();
  for (uint8_t col = 20; col < COLS; col++) {
    vfd.setCursor(col, 0);
    vfd.write(SLOT_FULL);
    vfd.setCursor(col, 1);
    vfd.write(SLOT_FULL);
  }
  for (uint8_t lvl = 0; lvl < 4; lvl++) {
    vfd.setBrightness((CU40025VFD::Brightness)lvl);
    vfd.setCursor(0, 0);
    vfd.print(F("Brightness"));
    vfd.setCursor(0, 1);
    vfd.print(F("BR "));
    vfd.print(labels[lvl]);
    Serial.print(F("  BR "));
    Serial.println(labels[lvl]);
    delay(2500);
  }

  vfd.setCursor(0, 1);
  vfd.print(F("Fade cycle  "));
  for (uint8_t cycle = 0; cycle < 3; cycle++) {
    for (int8_t lvl = 3; lvl >= 0; lvl--) { vfd.setBrightness((CU40025VFD::Brightness)lvl); delay(250); }
    for (uint8_t lvl = 1; lvl < 4; lvl++) { vfd.setBrightness((CU40025VFD::Brightness)lvl); delay(250); }
  }
  vfd.setBrightness(CU40025VFD::BRIGHTNESS_100);
  delay(500);
}

// Display off keeps DD RAM contents
void testDisplayOnOff() {
  header(7, F("Display ON/OFF"), F("D bit blanks screen, DD RAM retained"));

  vfd.clear();
  vfd.print(F("This text survives display OFF/ON."));
  vfd.setCursor(0, 1);
  vfd.print(F("Blanking: "));
  for (uint8_t i = 0; i < 4; i++) {
    vfd.noDisplay();
    delay(800);
    vfd.display();
    vfd.setCursor(10 + i * 3, 1);
    vfd.print(i + 1);
    vfd.write(' ');
    delay(800);
  }
  Serial.println(F("  Toggled 4 times"));
  delay(800);
}

// Cursor and blink, moved with the cursor shift instruction
void testCursor() {
  header(8, F("Cursor modes"), F("C=cursor, B=blink (~1.1 Hz), shift cmd"));

  static const struct { bool c, b; const char *label; } modes[3] = {
    { true, false, "C=1 B=0  underline cursor" },
    { false, true, "C=0 B=1  blinking character" },
    { true, true,  "C=1 B=1  cursor + blink" },
  };

  for (uint8_t m = 0; m < 3; m++) {
    vfd.setDisplayControl(true, false, false);
    vfd.clear();
    vfd.print(modes[m].label);
    vfd.setCursor(0, 1);
    vfd.print(F("The quick brown fox jumps over lazy dogs"));
    Serial.print(F("  "));
    Serial.println(modes[m].label);

    vfd.setCursor(4, 1);
    vfd.setDisplayControl(true, modes[m].c, modes[m].b);
    delay(3500);  // hold still so the blink rate is visible

    for (uint8_t i = 4; i < COLS - 1; i++) { vfd.moveCursorRight(); delay(70); }
    for (uint8_t i = COLS - 1; i > 0; i--) { vfd.moveCursorLeft(); delay(70); }
    delay(1000);
  }
  vfd.setDisplayControl(true, false, false);
}

void typeText(const char *s, bool reverse, uint16_t ms) {
  uint8_t n = strlen(s);
  for (uint8_t i = 0; i < n; i++) {
    vfd.write(s[reverse ? n - 1 - i : i]);
    delay(ms);
  }
}

// All four text direction / autoscroll combinations
void testEntryModes() {
  header(9, F("Entry mode set"), F("I/D direction and S auto display shift"));

  vfd.clear();
  vfd.print(F("I/D=1 S=0: cursor moves right"));
  vfd.setCursor(0, 1);
  vfd.cursor();
  typeText("Typed left to right, cursor follows ->", false, 60);
  Serial.println(F("  I/D=1 S=0 leftToRight"));
  delay(1500);

  // String fed reversed so it reads normally
  vfd.clear();
  vfd.print(F("I/D=0 S=0: cursor moves left"));
  vfd.setCursor(COLS - 1, 1);
  vfd.rightToLeft();
  typeText("<- Typed right to left, cursor follows", true, 60);
  Serial.println(F("  I/D=0 S=0 rightToLeft"));
  delay(1500);

  vfd.leftToRight();
  vfd.clear();
  vfd.print(F("I/D=1 S=1: both lines shift left"));
  vfd.setCursor(20, 1);
  vfd.autoscroll();
  typeText("Ticker scrolls LEFT", false, 250);
  Serial.println(F("  I/D=1 S=1 leftToRight + autoscroll"));
  delay(1500);

  vfd.noAutoscroll();
  vfd.clear();  // also resets the display shift
  vfd.print(F("I/D=0 S=1: both lines shift right"));
  vfd.setCursor(19, 1);
  vfd.rightToLeft();
  vfd.autoscroll();
  typeText("Ticker scrolls RIGHT", true, 250);
  Serial.println(F("  I/D=0 S=1 rightToLeft + autoscroll"));
  delay(1500);

  vfd.noAutoscroll();
  vfd.leftToRight();
  vfd.noCursor();
  vfd.clear();
}

// Display shift, cursor home, display clear
void testShiftHomeClear() {
  header(10, F("Shift/Home/Clear"), F("S/C=1 display shift, then home, clear"));

  vfd.clear();
  vfd.print(F(">>> Line 1: whole display shifts <<<   "));
  vfd.setCursor(0, 1);
  vfd.print(F("*** Line 2 moves in step with line 1 ***"));
  delay(1500);

  for (uint8_t i = 0; i < COLS; i++) { vfd.scrollDisplayLeft(); delay(90); }
  for (uint8_t i = 0; i < COLS; i++) { vfd.scrollDisplayRight(); delay(90); }
  Serial.println(F("  Display shifted 40 left (full wrap) and 40 right"));

  for (uint8_t i = 0; i < 12; i++) { vfd.scrollDisplayRight(); delay(90); }
  vfd.setCursor(20, 1);
  vfd.cursor();
  delay(2000);

  vfd.home();
  Serial.println(F("  Cursor home - shift reset, text kept"));
  delay(2500);

  vfd.noCursor();
  vfd.setCursor(0, 1);
  vfd.print(F("Cursor Home done - next: Display Clear  "));
  delay(2500);
  vfd.clear();
  Serial.println(F("  Display clear"));
  delay(1500);
}

void finale() {
  loadGlyphs(iconGlyphs);
  vfd.clear();
  vfd.write((uint8_t)0);
  vfd.print(F(" All documented features exercised "));
  vfd.write(1);
  vfd.setCursor(0, 1);
  vfd.print(F("Loop "));
  vfd.print(loopCount);
  vfd.print(F("  Read-back: "));
  vfd.print(!vfd.canRead() ? F("N/A ") : lastReadbackPass ? F("PASS") : F("FAIL"));
  vfd.print(F("  Restart "));
  for (int8_t s = 5; s > 0; s--) {
    vfd.setCursor(38, 1);
    vfd.print(s);
    delay(1000);
  }
  loadGlyphs(baseGlyphs);
  Serial.println(F("\n=== Loop complete ==="));
}

// ---------------------------------------------------------------------------
void setup() {
  Serial.begin(115200);
  bool readable = vfd.begin();
  Serial.print(F("Busy flag / read-back: "));
  Serial.println(readable ? F("working") : F("NOT working (fixed delays)"));
  loadGlyphs(baseGlyphs);
}

void loop() {
  // Continuous cycling also avoids phosphor burn-in from static text
  loopCount++;
  splash();
  testReadback();
  testAddressing();
  testCharset();
  testUserFonts();
  testAllDots();
  testBrightness();
  testDisplayOnOff();
  testCursor();
  testEntryModes();
  testShiftHomeClear();
  finale();
}
