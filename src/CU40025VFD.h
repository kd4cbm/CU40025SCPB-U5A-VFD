/*
 * CU40025VFD - Arduino driver for the Noritake Itron CU40025SCPB-U5A
 * 2 x 40 character vacuum fluorescent display (VFD).
 *
 * The CU40025SCPB-U5A is an OEM variant of the CU40025SCPB-W6J, built for
 * the AT&T / Lucent / Avaya 8434DX desk telephone. It uses the same
 * HD44780-style controller and instruction set as the W6J (Noritake spec
 * DS-805-0000-02), but it has NO on-board DC/DC converter. It needs a split
 * power supply:
 *
 *   U5A connector (numbered on the PCB)
 *   -----------------------------------
 *   Pin 1   GND            logic ground
 *   Pin 2   +5 V           logic supply (4.75 - 5.25 V)
 *   Pin 3   NC             no connection (per datasheet); leave open
 *   Pin 4   RS             register select: 0 = instruction, 1 = data
 *   Pin 5   R/W            M68 mode: 1 = read, 0 = write   (WR in i80 mode)
 *   Pin 6   E              M68 mode: enable strobe         (RD in i80 mode)
 *   Pin 7   DB0            data bus (unused in 4-bit mode)
 *   Pin 8   DB1            data bus (unused in 4-bit mode)
 *   Pin 9   DB2            data bus (unused in 4-bit mode)
 *   Pin 10  DB3            data bus (unused in 4-bit mode)
 *   Pin 11  DB4            data bus
 *   Pin 12  DB5            data bus
 *   Pin 13  DB6            data bus
 *   Pin 14  DB7            data bus (also the busy flag when reading)
 *   Pin 15  unknown        leave unconnected
 *   Pin 16  unknown        leave unconnected
 *   Pin 17  GND            high-voltage supply ground
 *   Pin 18  +18 V minimum  VFD drive supply (replaces the W6J's DC/DC)
 *
 * All logic inputs are 5 V TTL/CMOS. RS, R/W and E need >= 0.7 x VCC for a
 * logic high, so use 5 V boards, or level shifters on 3.3 V boards.
 *
 * This driver uses the M68 bus mode (RS, R/W, E), which is the factory
 * default (jumper JP2 open). It supports:
 *   - 8-bit or 4-bit data bus
 *   - optional R/W line: with R/W wired, the busy flag is polled and RAM can
 *     be read back; with R/W tied to GND, fixed delays are used instead
 *   - every instruction in the datasheet, including the 4-level brightness
 *     control that the HD44780 does not have
 *
 * The class derives from Print, so print(), println(), print(x, HEX) etc.
 * all work. Method names follow the Arduino LiquidCrystal library where the
 * feature exists there, so existing LCD code ports easily.
 */

#ifndef CU40025VFD_H
#define CU40025VFD_H

#include <Arduino.h>
#include <Print.h>

class CU40025VFD : public Print {
public:
  // Geometry
  static const uint8_t COLS = 40;
  static const uint8_t ROWS = 2;

  // Pass as the rw argument when R/W is tied to GND (write-only operation)
  static const uint8_t NO_PIN = 0xFF;

  // Brightness levels (datasheet 7.7.2)
  enum Brightness : uint8_t {
    BRIGHTNESS_100 = 0x00,  // power-on default
    BRIGHTNESS_75  = 0x01,
    BRIGHTNESS_50  = 0x02,
    BRIGHTNESS_25  = 0x03,
  };

  // Raw instruction codes (datasheet 7.1), for use with command()
  static const uint8_t CMD_CLEAR    = 0x01;
  static const uint8_t CMD_HOME     = 0x02;
  static const uint8_t CMD_ENTRY    = 0x04;  // | ENTRY_INCREMENT | ENTRY_SHIFT
  static const uint8_t CMD_DISPLAY  = 0x08;  // | DISPLAY_ON | DISPLAY_CURSOR | DISPLAY_BLINK
  static const uint8_t CMD_SHIFT    = 0x10;  // | SHIFT_DISPLAY | SHIFT_RIGHT
  static const uint8_t CMD_FUNCTION = 0x20;  // | FUNCTION_8BIT
  static const uint8_t CMD_CGRAM    = 0x40;  // | 6-bit CG RAM address
  static const uint8_t CMD_DDRAM    = 0x80;  // | 7-bit DD RAM address

  static const uint8_t ENTRY_INCREMENT = 0x02;
  static const uint8_t ENTRY_SHIFT     = 0x01;
  static const uint8_t DISPLAY_ON      = 0x04;
  static const uint8_t DISPLAY_CURSOR  = 0x02;
  static const uint8_t DISPLAY_BLINK   = 0x01;
  static const uint8_t SHIFT_DISPLAY   = 0x08;
  static const uint8_t SHIFT_RIGHT     = 0x04;
  static const uint8_t FUNCTION_8BIT   = 0x10;

  // DD RAM start address of each line (line 1: 00-27H, line 2: 40-67H)
  static const uint8_t LINE1_ADDR = 0x00;
  static const uint8_t LINE2_ADDR = 0x40;

  /*
   * 8-bit bus constructor.
   *   rs, rw, en : control pins. Pass NO_PIN for rw if R/W is tied to GND.
   *   d0 .. d7   : data pins DB0 .. DB7.
   */
  CU40025VFD(uint8_t rs, uint8_t rw, uint8_t en,
             uint8_t d0, uint8_t d1, uint8_t d2, uint8_t d3,
             uint8_t d4, uint8_t d5, uint8_t d6, uint8_t d7);

  /*
   * 4-bit bus constructor. Only DB4 .. DB7 are connected; leave DB0 .. DB3
   * on the display unconnected.
   *   rs, rw, en : control pins. Pass NO_PIN for rw if R/W is tied to GND.
   *   d4 .. d7   : data pins DB4 .. DB7.
   */
  CU40025VFD(uint8_t rs, uint8_t rw, uint8_t en,
             uint8_t d4, uint8_t d5, uint8_t d6, uint8_t d7);

  /*
   * Initialise the display. Call once in setup(), after both supplies are up.
   *
   * The display's power-on reset can fail if +5 V rises slowly (datasheet
   * 8.2), so this always initialises by command: bus width, brightness
   * 100 %, display cleared, increment mode, display on, cursor off.
   *
   * Returns true if the busy flag could be read back (R/W is wired and
   * working). Returns false if there is no R/W pin or reads failed; the
   * display still works, using fixed delays instead of busy-flag polling.
   */
  bool begin();

  // ---- Basic control ------------------------------------------------------

  // Clear all 80 characters, cursor to top left, display shift reset,
  // entry mode set to increment. Takes up to 1.8 ms.
  void clear();

  // Cursor to top left and undo any display shift. Text is not changed.
  void home();

  // Move the cursor to col (0-39), row (0-1). Out-of-range values are
  // clamped to the last column/row.
  void setCursor(uint8_t col, uint8_t row);

  // Turn the whole display on / off. Text in DD RAM is kept while off, and
  // the display draws much less current (datasheet: ~8 mA vs ~330 mA on the W6J).
  void display();
  void noDisplay();

  // Show / hide the underline cursor at the current position.
  void cursor();
  void noCursor();

  // Blink the character at the cursor position (about 1.1 Hz, alternating
  // between the character and all dots on).
  void blink();
  void noBlink();

  // Set display, cursor and blink together in one instruction.
  void setDisplayControl(bool displayOn, bool cursorOn, bool blinkOn);

  // ---- Brightness ---------------------------------------------------------

  // Set brightness to 100, 75, 50 or 25 %. This is sent as a Function Set
  // instruction followed immediately by a data byte (datasheet 7.7.2).
  void setBrightness(Brightness level);

  // Current brightness (the display itself cannot report it).
  Brightness getBrightness() const { return _brightness; }

  // ---- Scrolling and text direction ---------------------------------------

  // Shift both lines of the display one position left / right. DD RAM is
  // not changed; each line wraps around its 40 characters. home() or
  // clear() undoes the shift.
  void scrollDisplayLeft();
  void scrollDisplayRight();

  // Move the cursor one position left / right without writing.
  void moveCursorLeft();
  void moveCursorRight();

  // Text direction. leftToRight(): cursor moves right after each character
  // (default). rightToLeft(): cursor moves left after each character.
  void leftToRight();
  void rightToLeft();

  // autoscroll(): the display shifts on every write so the cursor stays in
  // the same place on screen (a "ticker"). Both lines shift together.
  void autoscroll();
  void noAutoscroll();

  // ---- User-defined characters (CG RAM) -----------------------------------

  /*
   * Define one of the 8 user characters. Character codes 0-7 and their
   * mirrors 8-15 then display it.
   *   slot : 0 - 7
   *   rows : 8 bytes. rows[0..6] are the 5x7 dot rows, top to bottom, with
   *          bit 4 the leftmost dot and bit 0 the rightmost. rows[7] turns
   *          the underline (dot 36) on when non-zero.
   * Characters already on screen that use this slot change immediately,
   * which can be used for animation. Afterwards the cursor is back in
   * DD RAM at the start of line 1, so call setCursor() before writing.
   */
  void createChar(uint8_t slot, const uint8_t rows[8]);

  // Same as createChar(), but rows[] is stored in PROGMEM (flash).
  void createChar_P(uint8_t slot, const uint8_t *rows);

  // ---- Reading back (needs R/W wired) -------------------------------------

  // True if begin() found a working busy flag, i.e. reads are possible.
  bool canRead() const { return _canRead; }

  // True if the display is busy executing an instruction.
  // Always false when reads are not possible.
  bool isBusy();

  // Current address counter (DD RAM or CG RAM address, 0 - 0x7F).
  // Returns 0 when reads are not possible.
  uint8_t readAddress();

  // Read the character code shown at col, row. Returns 0 when reads are
  // not possible. Moves the cursor to the next position.
  uint8_t readChar(uint8_t col, uint8_t row);

  /*
   * Read one row (0-7) of a user character from CG RAM. Only bits 0-4
   * (rows 0-6) or bit 2 (row 7, underline) are meaningful. Leaves the
   * cursor at the start of line 1.
   *
   * Note: consecutive CG RAM reads on this module skip addresses, so this
   * sets the CG RAM address before every read, as the datasheet requires.
   */
  uint8_t readCharRow(uint8_t slot, uint8_t row);

  // ---- Low level ----------------------------------------------------------

  // Send a raw instruction (RS = 0), waiting for the display to be ready.
  void command(uint8_t value);

  // Write one character code (RS = 1) at the cursor. Used by print().
  size_t write(uint8_t value) override;
  using Print::write;

  // Read one byte of data (RS = 1) from the current DD/CG RAM address.
  // Returns 0 when reads are not possible.
  uint8_t readData();

  // Read the busy flag (bit 7) and address counter (bits 0-6) in one go.
  // Returns 0 when reads are not possible.
  uint8_t readStatus();

  // Wait until the display can accept the next instruction.
  void waitReady();

private:
  uint8_t _rs, _rw, _en;
  uint8_t _data[8];
  bool _fourBit;
  bool _canRead = false;
  bool _busIsOutput = false;
  uint8_t _displayControl = DISPLAY_ON;
  uint8_t _entryMode = ENTRY_INCREMENT;
  Brightness _brightness = BRIGHTNESS_100;

  void busOutput();
  void busInput();
  void pulseEnable();
  void writeNibble(uint8_t nibble);
  void writeBus(bool isData, uint8_t value);
  uint8_t readBus(bool isData);
  bool probeBusyFlag();
};

#endif
