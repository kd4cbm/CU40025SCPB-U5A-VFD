// CU40025SCPB-U5A (Noritake Itron 2x40 VFD) - full feature demo / test
// Written from the CU40025SCPB-W6J datasheet (same controller/instruction set).
//
// Exercises every feature documented in spec DS-805-0000-02:
//   7.2  Display clear          7.8  CG RAM address / user fonts (8.1)
//   7.3  Cursor home            7.9  DD RAM address (both lines, all 80 cells)
//   7.4  Entry mode (I/D, S)    7.10 Write data
//   7.5  Display/cursor/blink   7.11 Read data (DD RAM + CG RAM read-back)
//   7.6  Cursor/display shift   7.12 Busy flag / address counter read
//   7.7  Function set + brightness control (100/75/50/25%)
//   9    Full character font (all 256 codes)
//
// Bus: 8-bit, M68 mode (JP2 open, the factory default): RS, R/W, E.
// Progress is also logged on Serial at 115200 baud.

const int rs = 2, rw = 3, en = 4, d0 = 5, d1 = 6, d2 = 7, d3 = 8, d4 = 9, d5 = 10, d6 = 11, d7 = 12;
const uint8_t dataPins[8] = { d0, d1, d2, d3, d4, d5, d6, d7 };

const uint8_t COLS = 40;
const uint8_t NUM_TESTS = 10;

// Instruction set (datasheet 7.1)
const uint8_t CMD_CLEAR    = 0x01;
const uint8_t CMD_HOME     = 0x02;
const uint8_t CMD_ENTRY    = 0x04;  // | I/D 0x02 | S 0x01
const uint8_t CMD_DISPLAY  = 0x08;  // | D 0x04 | C 0x02 | B 0x01
const uint8_t CMD_SHIFT    = 0x10;  // | S/C 0x08 | R/L 0x04
const uint8_t CMD_FUNCTION = 0x20;  // | IF 0x10 (8-bit)
const uint8_t CMD_CGRAM    = 0x40;
const uint8_t CMD_DDRAM    = 0x80;

// ---------------------------------------------------------------------------
// Low-level driver
// ---------------------------------------------------------------------------
class VFD : public Print {
public:
  bool busyFlagWorks = false;

  void begin() {
    pinMode(rs, OUTPUT);
    pinMode(rw, OUTPUT);
    pinMode(en, OUTPUT);
    digitalWrite(en, LOW);
    digitalWrite(rw, LOW);
    digitalWrite(rs, LOW);
    busOutput();
    delay(100);

    // Power-on reset may fail with slow supply rise (8.2), so init by command.
    // Function set must be the first instruction (7.7.1).
    for (uint8_t i = 0; i < 3; i++) {
      writeRaw(false, CMD_FUNCTION | 0x10);
      delay(2);
    }
    writeRaw(true, 0x00);  // brightness byte immediately after function set: 100%
    delay(2);

    busyFlagWorks = probeBusyFlag();

    command(CMD_DISPLAY);  // display off while initialising
    clear();
    entryMode(true, false);
    display(true, false, false);
  }

  // Sets DD RAM address 0x45 and checks that BF=0 and AC reads back 0x45.
  bool probeBusyFlag() {
    writeRaw(false, CMD_DDRAM | 0x45);
    delay(2);
    uint8_t s = readRaw(false);
    writeRaw(false, CMD_DDRAM);
    delay(2);
    return s == 0x45;
  }

  void waitReady() {
    if (!busyFlagWorks) {
      delayMicroseconds(10);
      return;
    }
    unsigned long start = micros();
    while (readRaw(false) & 0x80) {
      if (micros() - start > 10000) {
        busyFlagWorks = false;
        Serial.println(F("!! Busy flag stuck high - falling back to fixed delays"));
        return;
      }
    }
  }

  void command(uint8_t c) {
    waitReady();
    writeRaw(false, c);
    if (!busyFlagWorks && (c == CMD_CLEAR || (c & 0xFE) == CMD_HOME)) delay(2);
  }

  size_t write(uint8_t c) override {
    waitReady();
    writeRaw(true, c);
    return 1;
  }
  using Print::write;

  uint8_t readStatus() { return readRaw(false); }  // BF (bit 7) + address counter

  uint8_t readData() {
    waitReady();
    return readRaw(true);
  }

  void clear() { command(CMD_CLEAR); }
  void home() { command(CMD_HOME); }
  void setCursor(uint8_t col, uint8_t row) { command(CMD_DDRAM | (row ? 0x40 : 0x00) | col); }
  void entryMode(bool increment, bool shift) { command(CMD_ENTRY | (increment ? 0x02 : 0) | (shift ? 0x01 : 0)); }
  void display(bool on, bool cursor, bool blink) {
    command(CMD_DISPLAY | (on ? 0x04 : 0) | (cursor ? 0x02 : 0) | (blink ? 0x01 : 0));
  }
  void shiftCursor(bool right) { command(CMD_SHIFT | (right ? 0x04 : 0)); }
  void shiftDisplay(bool right) { command(CMD_SHIFT | 0x08 | (right ? 0x04 : 0)); }

  // level: 0=100%, 1=75%, 2=50%, 3=25% (7.7.2). The data byte must directly
  // follow Function Set, so no busy-flag read is inserted between them.
  void brightness(uint8_t level) {
    command(CMD_FUNCTION | 0x10);
    delayMicroseconds(5);
    writeRaw(true, level & 0x03);
    delayMicroseconds(5);
  }

  // rows[0..6] = 5x7 dots, rows[7] = underline (dot 36)
  void createChar(uint8_t slot, const uint8_t rows[8]) {
    command(CMD_CGRAM | ((slot & 7) << 3));
    for (uint8_t i = 0; i < 8; i++) write(rows[i]);
    command(CMD_DDRAM);  // datasheet 7.2 caution: leave the AC pointing at DD RAM
  }

  void fillLine(uint8_t row, uint8_t c) {
    setCursor(0, row);
    for (uint8_t i = 0; i < COLS; i++) write(c);
  }

private:
  bool busIsOutput = false;

  void busOutput() {
    if (busIsOutput) return;
    for (uint8_t i = 0; i < 8; i++) pinMode(dataPins[i], OUTPUT);
    busIsOutput = true;
  }

  void busInput() {
    if (!busIsOutput) return;
    for (uint8_t i = 0; i < 8; i++) pinMode(dataPins[i], INPUT);
    busIsOutput = false;
  }

  void writeRaw(bool isData, uint8_t v) {
    digitalWrite(rw, LOW);  // module stops driving before we take the bus
    digitalWrite(rs, isData);
    busOutput();
    for (uint8_t i = 0; i < 8; i++) digitalWrite(dataPins[i], (v >> i) & 1);
    digitalWrite(en, HIGH);
    delayMicroseconds(1);
    digitalWrite(en, LOW);
    delayMicroseconds(1);
  }

  uint8_t readRaw(bool isData) {
    busInput();  // release the bus before the module drives it
    digitalWrite(rs, isData);
    digitalWrite(rw, HIGH);
    delayMicroseconds(1);
    digitalWrite(en, HIGH);
    delayMicroseconds(1);
    uint8_t v = 0;
    for (uint8_t i = 0; i < 8; i++) v |= digitalRead(dataPins[i]) << i;
    digitalWrite(en, LOW);
    digitalWrite(rw, LOW);
    delayMicroseconds(1);
    return v;
  }
};

VFD vfd;

// ---------------------------------------------------------------------------
// User font data (5x7 + underline row, datasheet 8.1)
// ---------------------------------------------------------------------------
// Base set, kept loaded between tests
const uint8_t SLOT_FULL = 5, SLOT_CHECK_A = 6, SLOT_CHECK_B = 7;
const uint8_t baseGlyphs[8][8] = {
  { 0x10, 0x10, 0x10, 0x10, 0x10, 0x10, 0x10, 0x00 },  // 0-4: bar 1..5 columns wide
  { 0x18, 0x18, 0x18, 0x18, 0x18, 0x18, 0x18, 0x00 },
  { 0x1C, 0x1C, 0x1C, 0x1C, 0x1C, 0x1C, 0x1C, 0x00 },
  { 0x1E, 0x1E, 0x1E, 0x1E, 0x1E, 0x1E, 0x1E, 0x00 },
  { 0x1F, 0x1F, 0x1F, 0x1F, 0x1F, 0x1F, 0x1F, 0x00 },
  { 0x1F, 0x1F, 0x1F, 0x1F, 0x1F, 0x1F, 0x1F, 0x1F },  // 5: all 36 dots on
  { 0x15, 0x0A, 0x15, 0x0A, 0x15, 0x0A, 0x15, 0x00 },  // 6: checker
  { 0x0A, 0x15, 0x0A, 0x15, 0x0A, 0x15, 0x0A, 0x1F },  // 7: inverse checker
};

const uint8_t iconGlyphs[8][8] = {
  { 0x00, 0x0A, 0x0A, 0x00, 0x11, 0x0E, 0x00, 0x00 },  // smiley
  { 0x00, 0x0A, 0x1F, 0x1F, 0x0E, 0x04, 0x00, 0x00 },  // heart
  { 0x04, 0x0E, 0x0E, 0x0E, 0x1F, 0x00, 0x04, 0x00 },  // bell
  { 0x02, 0x03, 0x02, 0x02, 0x0E, 0x1E, 0x0C, 0x00 },  // note
  { 0x0E, 0x11, 0x11, 0x1F, 0x1B, 0x1B, 0x1F, 0x00 },  // padlock
  { 0x18, 0x18, 0x07, 0x08, 0x08, 0x08, 0x07, 0x00 },  // degrees C
  { 0x04, 0x0E, 0x15, 0x04, 0x04, 0x04, 0x04, 0x00 },  // up arrow
  { 0x0E, 0x11, 0x11, 0x1F, 0x11, 0x11, 0x11, 0x1F },  // 'A' with underline (dot 36)
};

const uint8_t pacOpen[8]   = { 0x0E, 0x1B, 0x1E, 0x1C, 0x1E, 0x1F, 0x0E, 0x00 };
const uint8_t pacClosed[8] = { 0x0E, 0x1B, 0x1F, 0x1F, 0x1F, 0x1F, 0x0E, 0x00 };

void loadGlyphs(const uint8_t set[8][8]) {
  for (uint8_t i = 0; i < 8; i++) vfd.createChar(i, set[i]);
}

// ---------------------------------------------------------------------------
// Helpers
// ---------------------------------------------------------------------------
uint16_t loopCount = 0;
bool lastReadbackPass = false;

void header(uint8_t n, const __FlashStringHelper *name, const __FlashStringHelper *detail) {
  vfd.display(true, false, false);
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

// 7.11 / 7.12: busy flag, address counter, DD RAM and CG RAM read-back
void testReadback() {
  header(1, F("Bus read-back"), F("BF/AC read, DD RAM + CG RAM verify"));

  if (!vfd.busyFlagWorks) {
    printPadded(0, F("Read-back FAILED: busy flag/AC unreadable"));
    printPadded(1, F("Check R/W (pin 3). Using fixed delays."));
    Serial.println(F("  Busy flag probe failed - reads not possible"));
    lastReadbackPass = false;
    delay(4000);
    return;
  }

  // Fill all 80 cells with a known pattern (visible on screen)
  for (uint8_t row = 0; row < 2; row++) {
    vfd.setCursor(0, row);
    for (uint8_t col = 0; col < COLS; col++) vfd.write(0x21 + ((row * COLS + col) * 7) % 94);
  }
  delay(1500);

  uint8_t ddOk = 0;
  for (uint8_t row = 0; row < 2; row++) {
    vfd.setCursor(0, row);
    for (uint8_t col = 0; col < COLS; col++) {
      uint8_t expect = 0x21 + ((row * COLS + col) * 7) % 94;
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

  // Address counter: set 0x51 (line 2, col 17) and read it back
  vfd.setCursor(17, 1);
  vfd.waitReady();
  uint8_t ac = vfd.readStatus() & 0x7F;
  bool acOk = (ac == 0x51);

  // CG RAM: write 64 bytes and read back (only displayed bits compared).
  // Sequential CG RAM reads on this module skip addresses, so the address is
  // set before every read, as 7.11 requires.
  vfd.command(CMD_CGRAM);
  for (uint8_t i = 0; i < 64; i++) vfd.write((i * 5 + 3) & 0x1F);
  uint8_t cgOk = 0;
  for (uint8_t i = 0; i < 64; i++) {
    uint8_t mask = ((i & 7) == 7) ? 0x04 : 0x1F;  // row 8: only dot 36 (DB2) is defined
    vfd.command(CMD_CGRAM | i);
    uint8_t got = vfd.readData();
    if ((got & mask) == (((i * 5 + 3) & 0x1F) & mask)) cgOk++;
    else {
      Serial.print(F("  CG RAM mismatch @"));
      Serial.print(i);
      Serial.print(F(" exp 0x"));
      Serial.print((i * 5 + 3) & 0x1F, HEX);
      Serial.print(F(" got 0x"));
      Serial.println(got, HEX);
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

// 7.9: every DD RAM address on both lines
void testAddressing() {
  header(2, F("DD RAM addressing"), F("80 cells: 00-27H line 1, 40-67H line 2"));

  vfd.clear();
  vfd.setCursor(0, 0);
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

  // Fill each cell in address order, line 2 filled right-to-left by address
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

// 9: all 256 character codes, 64 per page
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

// 8.1: user definable fonts, underline dot, live CG RAM update
void testUserFonts() {
  header(4, F("User fonts (CG RAM)"), F("8 chars, 5x7 + underline dot 36"));

  loadGlyphs(iconGlyphs);
  vfd.clear();
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
  vfd.fillLine(1, '.');
  for (uint8_t col = 0; col < COLS; col++) {
    vfd.createChar(7, (col & 1) ? pacClosed : pacOpen);
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
  vfd.fillLine(1, ' ');
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

// All dots on and checker patterns (software equivalent of test mode 8.4)
void testAllDots() {
  header(5, F("All dots / checker"), F("Every dot incl. underline (max current)"));

  vfd.fillLine(0, SLOT_FULL);
  vfd.fillLine(1, SLOT_FULL);
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

// 7.7.2: brightness 100 / 75 / 50 / 25 %
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
    vfd.brightness(lvl);
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
    for (int8_t lvl = 3; lvl >= 0; lvl--) { vfd.brightness(lvl); delay(250); }
    for (uint8_t lvl = 1; lvl < 4; lvl++) { vfd.brightness(lvl); delay(250); }
  }
  vfd.brightness(0);
  delay(500);
}

// 7.5: D bit - display off keeps DD RAM contents
void testDisplayOnOff() {
  header(7, F("Display ON/OFF"), F("D bit blanks screen, DD RAM retained"));

  vfd.clear();
  vfd.print(F("This text survives display OFF/ON."));
  vfd.setCursor(0, 1);
  vfd.print(F("Blanking: "));
  for (uint8_t i = 0; i < 4; i++) {
    vfd.display(false, false, false);
    delay(800);
    vfd.display(true, false, false);
    vfd.setCursor(10 + i * 3, 1);
    vfd.print(i + 1);
    vfd.write(' ');
    delay(800);
  }
  Serial.println(F("  Toggled 4 times"));
  delay(800);
}

// 7.5 C and B bits, moved with 7.6 cursor shift
void testCursor() {
  header(8, F("Cursor modes"), F("C=cursor, B=blink (~1.1 Hz), shift cmd"));

  static const struct { bool c, b; const char *label; } modes[3] = {
    { true, false, "C=1 B=0  underline cursor               " },
    { false, true, "C=0 B=1  blinking character             " },
    { true, true,  "C=1 B=1  cursor + blink                 " },
  };

  for (uint8_t m = 0; m < 3; m++) {
    vfd.display(true, false, false);
    vfd.clear();
    vfd.print(modes[m].label);
    vfd.setCursor(0, 1);
    vfd.print(F("The quick brown fox jumps over lazy dogs"));
    Serial.print(F("  "));
    Serial.println(modes[m].label);

    vfd.setCursor(4, 1);
    vfd.display(true, modes[m].c, modes[m].b);
    delay(3500);  // hold still so the blink rate is visible

    // Cursor-only shift (S/C=0): right across the line, then back left
    for (uint8_t i = 4; i < COLS - 1; i++) { vfd.shiftCursor(true); delay(70); }
    for (uint8_t i = COLS - 1; i > 0; i--) { vfd.shiftCursor(false); delay(70); }
    delay(1000);
  }
  vfd.display(true, false, false);
}

void typeText(const char *s, bool reverse, uint16_t ms) {
  uint8_t n = strlen(s);
  for (uint8_t i = 0; i < n; i++) {
    vfd.write(s[reverse ? n - 1 - i : i]);
    delay(ms);
  }
}

// 7.4: all four I/D, S combinations
void testEntryModes() {
  header(9, F("Entry mode set"), F("I/D direction and S auto display shift"));

  // I/D=1 S=0
  vfd.clear();
  vfd.print(F("I/D=1 S=0: cursor moves right"));
  vfd.setCursor(0, 1);
  vfd.display(true, true, false);
  vfd.entryMode(true, false);
  typeText("Typed left to right, cursor follows ->", false, 60);
  Serial.println(F("  I/D=1 S=0"));
  delay(1500);

  // I/D=0 S=0: write from the right edge; string fed reversed so it reads normally
  vfd.entryMode(true, false);
  vfd.clear();
  vfd.print(F("I/D=0 S=0: cursor moves left"));
  vfd.setCursor(COLS - 1, 1);
  vfd.entryMode(false, false);
  typeText("<- Typed right to left, cursor follows", true, 60);
  Serial.println(F("  I/D=0 S=0"));
  delay(1500);

  // I/D=1 S=1: display shifts left on every write, cursor stays put on screen
  vfd.entryMode(true, false);
  vfd.clear();
  vfd.print(F("I/D=1 S=1: both lines shift left"));
  vfd.setCursor(20, 1);
  vfd.entryMode(true, true);
  typeText("Ticker scrolls LEFT", false, 250);
  Serial.println(F("  I/D=1 S=1"));
  delay(1500);

  // I/D=0 S=1: display shifts right on every write
  vfd.entryMode(true, false);
  vfd.clear();  // clear also resets the display shift
  vfd.print(F("I/D=0 S=1: both lines shift right"));
  vfd.setCursor(19, 1);
  vfd.entryMode(false, true);
  typeText("Ticker scrolls RIGHT", true, 250);
  Serial.println(F("  I/D=0 S=1"));
  delay(1500);

  vfd.entryMode(true, false);
  vfd.display(true, false, false);
  vfd.clear();
}

// 7.6 display shift, 7.3 cursor home, 7.2 display clear
void testShiftHomeClear() {
  header(10, F("Shift/Home/Clear"), F("S/C=1 display shift, then home, clear"));

  vfd.clear();
  vfd.print(F(">>> Line 1: whole display shifts <<<   "));
  vfd.setCursor(0, 1);
  vfd.print(F("*** Line 2 moves in step with line 1 ***"));
  delay(1500);

  for (uint8_t i = 0; i < COLS; i++) { vfd.shiftDisplay(false); delay(90); }
  for (uint8_t i = 0; i < COLS; i++) { vfd.shiftDisplay(true); delay(90); }
  Serial.println(F("  Display shifted 40 left (full wrap) and 40 right"));

  for (uint8_t i = 0; i < 12; i++) { vfd.shiftDisplay(true); delay(90); }
  vfd.setCursor(20, 1);
  vfd.display(true, true, false);
  delay(2000);

  // Home: undoes the shift and moves the cursor to 0, DD RAM untouched
  vfd.home();
  Serial.println(F("  Cursor home - shift reset, text kept"));
  delay(2500);

  vfd.display(true, false, false);
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
  vfd.write((uint8_t)0); vfd.print(F(" All documented features exercised "));
  vfd.write(1);
  vfd.setCursor(0, 1);
  vfd.print(F("Loop "));
  vfd.print(loopCount);
  vfd.print(F("  Read-back: "));
  vfd.print(!vfd.busyFlagWorks ? F("N/A ") : lastReadbackPass ? F("PASS") : F("FAIL"));
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
  vfd.begin();
  Serial.print(F("Busy flag / read-back: "));
  Serial.println(vfd.busyFlagWorks ? F("working") : F("NOT working (fixed delays)"));
  loadGlyphs(baseGlyphs);
}

void loop() {
  // Continuous cycling also avoids phosphor burn-in from static text (datasheet precautions)
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
