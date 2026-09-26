/*
 * CU40025VFD - Arduino driver for the Noritake Itron CU40025SCPB-U5A VFD.
 * See CU40025VFD.h for wiring, power and API documentation.
 */

#include "CU40025VFD.h"

// Fallback delay per instruction when the busy flag can't be read. The
// datasheet cycle time is 666 ns for everything except Display Clear.
static const uint8_t FIXED_DELAY_US = 20;

// Longest instruction (Display Clear) is 1.8 ms max; allow margin.
static const unsigned long BUSY_TIMEOUT_US = 10000;

CU40025VFD::CU40025VFD(uint8_t rs, uint8_t rw, uint8_t en,
                       uint8_t d0, uint8_t d1, uint8_t d2, uint8_t d3,
                       uint8_t d4, uint8_t d5, uint8_t d6, uint8_t d7)
    : _rs(rs), _rw(rw), _en(en), _data{ d0, d1, d2, d3, d4, d5, d6, d7 }, _fourBit(false) {}

CU40025VFD::CU40025VFD(uint8_t rs, uint8_t rw, uint8_t en,
                       uint8_t d4, uint8_t d5, uint8_t d6, uint8_t d7)
    : _rs(rs), _rw(rw), _en(en), _data{ NO_PIN, NO_PIN, NO_PIN, NO_PIN, d4, d5, d6, d7 }, _fourBit(true) {}

bool CU40025VFD::begin() {
  pinMode(_rs, OUTPUT);
  pinMode(_en, OUTPUT);
  digitalWrite(_rs, LOW);
  digitalWrite(_en, LOW);
  if (_rw != NO_PIN) {
    pinMode(_rw, OUTPUT);
    digitalWrite(_rw, LOW);
  }
  _busIsOutput = false;
  busOutput();
  _canRead = false;
  delay(100);  // let the module's own power-on reset finish

  // Force a known bus width. Three "8-bit function set" nibbles resync the
  // module whether it is in 8-bit mode or part-way through a 4-bit transfer
  // (e.g. after the Arduino was reset but the display was not).
  for (uint8_t i = 0; i < 3; i++) {
    if (_fourBit) writeNibble(0x3);
    else writeBus(false, CMD_FUNCTION | FUNCTION_8BIT);
    delay(5);
  }
  if (_fourBit) {
    writeNibble(0x2);  // switch to 4-bit; from here on bytes go as two nibbles
    delay(5);
  }

  // Function Set must be the first full instruction (datasheet 7.7.1); the
  // brightness byte that follows it also resets brightness to 100 %.
  setBrightness(BRIGHTNESS_100);
  delay(1);

  _canRead = (_rw != NO_PIN) && probeBusyFlag();

  command(CMD_DISPLAY);  // display off while clearing
  clear();
  _entryMode = ENTRY_INCREMENT;
  command(CMD_ENTRY | _entryMode);
  _displayControl = DISPLAY_ON;
  command(CMD_DISPLAY | _displayControl);
  return _canRead;
}

// Set DD RAM address 0x45, then read the status. A working bus returns
// BF = 0 and address 0x45; a missing/broken R/W line returns junk.
bool CU40025VFD::probeBusyFlag() {
  writeBus(false, CMD_DDRAM | 0x45);
  delay(1);
  uint8_t status = readBus(false);
  writeBus(false, CMD_DDRAM);
  delay(1);
  return status == 0x45;
}

// ---- Basic control --------------------------------------------------------

void CU40025VFD::clear() {
  command(CMD_CLEAR);
  _entryMode |= ENTRY_INCREMENT;  // Display Clear also selects increment (7.2)
}

void CU40025VFD::home() { command(CMD_HOME); }

void CU40025VFD::setCursor(uint8_t col, uint8_t row) {
  if (col >= COLS) col = COLS - 1;
  if (row >= ROWS) row = ROWS - 1;
  command(CMD_DDRAM | ((row ? LINE2_ADDR : LINE1_ADDR) + col));
}

void CU40025VFD::display() { setDisplayControl(true, _displayControl & DISPLAY_CURSOR, _displayControl & DISPLAY_BLINK); }
void CU40025VFD::noDisplay() { setDisplayControl(false, _displayControl & DISPLAY_CURSOR, _displayControl & DISPLAY_BLINK); }
void CU40025VFD::cursor() { setDisplayControl(_displayControl & DISPLAY_ON, true, _displayControl & DISPLAY_BLINK); }
void CU40025VFD::noCursor() { setDisplayControl(_displayControl & DISPLAY_ON, false, _displayControl & DISPLAY_BLINK); }
void CU40025VFD::blink() { setDisplayControl(_displayControl & DISPLAY_ON, _displayControl & DISPLAY_CURSOR, true); }
void CU40025VFD::noBlink() { setDisplayControl(_displayControl & DISPLAY_ON, _displayControl & DISPLAY_CURSOR, false); }

void CU40025VFD::setDisplayControl(bool displayOn, bool cursorOn, bool blinkOn) {
  _displayControl = (displayOn ? DISPLAY_ON : 0) | (cursorOn ? DISPLAY_CURSOR : 0) | (blinkOn ? DISPLAY_BLINK : 0);
  command(CMD_DISPLAY | _displayControl);
}

// ---- Brightness -------------------------------------------------------------

void CU40025VFD::setBrightness(Brightness level) {
  _brightness = (Brightness)(level & 0x03);
  command(CMD_FUNCTION | (_fourBit ? 0 : FUNCTION_8BIT));
  // The data byte must be the very next access, so no busy-flag read here.
  delayMicroseconds(5);
  writeBus(true, _brightness);
  delayMicroseconds(5);
}

// ---- Scrolling and text direction -----------------------------------------

void CU40025VFD::scrollDisplayLeft() { command(CMD_SHIFT | SHIFT_DISPLAY); }
void CU40025VFD::scrollDisplayRight() { command(CMD_SHIFT | SHIFT_DISPLAY | SHIFT_RIGHT); }
void CU40025VFD::moveCursorLeft() { command(CMD_SHIFT); }
void CU40025VFD::moveCursorRight() { command(CMD_SHIFT | SHIFT_RIGHT); }

void CU40025VFD::leftToRight() {
  _entryMode |= ENTRY_INCREMENT;
  command(CMD_ENTRY | _entryMode);
}

void CU40025VFD::rightToLeft() {
  _entryMode &= ~ENTRY_INCREMENT;
  command(CMD_ENTRY | _entryMode);
}

void CU40025VFD::autoscroll() {
  _entryMode |= ENTRY_SHIFT;
  command(CMD_ENTRY | _entryMode);
}

void CU40025VFD::noAutoscroll() {
  _entryMode &= ~ENTRY_SHIFT;
  command(CMD_ENTRY | _entryMode);
}

// ---- User-defined characters ----------------------------------------------

void CU40025VFD::createChar(uint8_t slot, const uint8_t rows[8]) {
  command(CMD_CGRAM | ((slot & 7) << 3));
  for (uint8_t i = 0; i < 7; i++) write(rows[i] & 0x1F);
  write(rows[7] ? 0x1F : 0x00);
  command(CMD_DDRAM);  // leave the address counter pointing at DD RAM (7.2)
}

void CU40025VFD::createChar_P(uint8_t slot, const uint8_t *rows) {
  uint8_t buf[8];
  for (uint8_t i = 0; i < 8; i++) buf[i] = pgm_read_byte(rows + i);
  createChar(slot, buf);
}

// ---- Reading back ---------------------------------------------------------

bool CU40025VFD::isBusy() { return readStatus() & 0x80; }

uint8_t CU40025VFD::readAddress() {
  waitReady();
  return readStatus() & 0x7F;
}

uint8_t CU40025VFD::readChar(uint8_t col, uint8_t row) {
  if (!_canRead) return 0;
  setCursor(col, row);
  return readData();
}

uint8_t CU40025VFD::readCharRow(uint8_t slot, uint8_t row) {
  if (!_canRead) return 0;
  command(CMD_CGRAM | ((slot & 7) << 3) | (row & 7));
  uint8_t value = readData();
  command(CMD_DDRAM);
  return value;
}

// ---- Low level ------------------------------------------------------------

void CU40025VFD::command(uint8_t value) {
  waitReady();
  writeBus(false, value);
  if (!_canRead && (value == CMD_CLEAR || (value & 0xFE) == CMD_HOME)) delay(2);
}

size_t CU40025VFD::write(uint8_t value) {
  waitReady();
  writeBus(true, value);
  return 1;
}

uint8_t CU40025VFD::readData() {
  if (!_canRead) return 0;
  waitReady();
  return readBus(true);
}

uint8_t CU40025VFD::readStatus() {
  if (!_canRead) return 0;
  return readBus(false);
}

void CU40025VFD::waitReady() {
  if (!_canRead) {
    delayMicroseconds(FIXED_DELAY_US);
    return;
  }
  unsigned long start = micros();
  while (readBus(false) & 0x80) {
    if (micros() - start > BUSY_TIMEOUT_US) {
      _canRead = false;  // busy flag stuck: fall back to fixed delays
      return;
    }
  }
}

void CU40025VFD::busOutput() {
  if (_busIsOutput) return;
  for (uint8_t i = _fourBit ? 4 : 0; i < 8; i++) pinMode(_data[i], OUTPUT);
  _busIsOutput = true;
}

void CU40025VFD::busInput() {
  if (!_busIsOutput) return;
  for (uint8_t i = _fourBit ? 4 : 0; i < 8; i++) pinMode(_data[i], INPUT);
  _busIsOutput = false;
}

void CU40025VFD::pulseEnable() {
  digitalWrite(_en, HIGH);
  delayMicroseconds(1);
  digitalWrite(_en, LOW);
  delayMicroseconds(1);
}

// Put a nibble on DB4-DB7 and strobe E. RS/R/W must already be set.
void CU40025VFD::writeNibble(uint8_t nibble) {
  busOutput();
  for (uint8_t i = 0; i < 4; i++) digitalWrite(_data[4 + i], (nibble >> i) & 1);
  pulseEnable();
}

void CU40025VFD::writeBus(bool isData, uint8_t value) {
  if (_rw != NO_PIN) digitalWrite(_rw, LOW);  // module releases the bus first
  digitalWrite(_rs, isData);
  if (_fourBit) {
    writeNibble(value >> 4);  // high nibble first (datasheet 8.3.2)
    writeNibble(value & 0x0F);
  } else {
    busOutput();
    for (uint8_t i = 0; i < 8; i++) digitalWrite(_data[i], (value >> i) & 1);
    pulseEnable();
  }
}

uint8_t CU40025VFD::readBus(bool isData) {
  busInput();  // release the bus before the module drives it
  digitalWrite(_rs, isData);
  digitalWrite(_rw, HIGH);
  delayMicroseconds(1);
  uint8_t value = 0;
  for (uint8_t pass = 0; pass < (_fourBit ? 2 : 1); pass++) {
    digitalWrite(_en, HIGH);
    delayMicroseconds(1);
    if (_fourBit) {
      value <<= 4;
      for (uint8_t i = 0; i < 4; i++) value |= digitalRead(_data[4 + i]) << i;
    } else {
      for (uint8_t i = 0; i < 8; i++) value |= digitalRead(_data[i]) << i;
    }
    digitalWrite(_en, LOW);
    delayMicroseconds(1);
  }
  digitalWrite(_rw, LOW);
  return value;
}
