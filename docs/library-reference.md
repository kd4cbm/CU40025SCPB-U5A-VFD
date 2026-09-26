# CU40025VFD library reference

```cpp
#include <CU40025VFD.h>
```

`CU40025VFD` derives from Arduino's `Print`, so `print()`, `println()`,
`print(value, HEX)`, `print(F("..."))` and so on all write to the display at
the cursor. Function names match Arduino's `LiquidCrystal` wherever that
library has the same feature.

Rows and columns count from 0: columns 0–39, rows 0 (top) and 1 (bottom).

## Constructors

```cpp
// 8-bit bus
CU40025VFD(uint8_t rs, uint8_t rw, uint8_t en,
           uint8_t d0, uint8_t d1, uint8_t d2, uint8_t d3,
           uint8_t d4, uint8_t d5, uint8_t d6, uint8_t d7);

// 4-bit bus (DB4-DB7 only)
CU40025VFD(uint8_t rs, uint8_t rw, uint8_t en,
           uint8_t d4, uint8_t d5, uint8_t d6, uint8_t d7);
```

Pass `CU40025VFD::NO_PIN` as `rw` if R/W is tied to GND. The display then
runs write-only, with fixed delays instead of busy-flag polling.

| Wiring | Arduino pins |
|---|---|
| 8-bit, with R/W | 11 |
| 8-bit, write-only | 10 |
| 4-bit, with R/W | 7 |
| 4-bit, write-only | 6 |

## Setup

### `bool begin()`
Initializes the display: sets the bus width, sets brightness to 100 %,
clears the screen, sets left-to-right entry, turns the display on with the
cursor off. Waits 100 ms first so that the display's own reset can finish.

Returns `true` if the busy flag could be read, meaning R/W is wired and
reading works. Returns `false` if there is no R/W pin or the read test
failed; writing still works either way.

## Text and cursor

| Function | Description |
|---|---|
| `clear()` | Blank all 80 characters, cursor to (0,0), undo any shift. Up to 1.8 ms. |
| `home()` | Cursor to (0,0) and undo any shift; text unchanged. |
| `setCursor(col, row)` | Move the cursor. Out-of-range values are clamped. |
| `write(code)` | Write one character code (0–255) and advance the cursor. |
| `print(...)` / `println(...)` | Anything `Print` supports. Note that `println` sends CR/LF as character codes, so use `setCursor()` to change line. |

## Display, cursor and blink

| Function | Description |
|---|---|
| `display()` / `noDisplay()` | Whole display on/off. Text is kept while off. |
| `cursor()` / `noCursor()` | Underline cursor on/off. |
| `blink()` / `noBlink()` | Blink the character at the cursor (~1.1 Hz). |
| `setDisplayControl(on, cursor, blink)` | Set all three with a single instruction. |

## Brightness

```cpp
vfd.setBrightness(CU40025VFD::BRIGHTNESS_75);
CU40025VFD::Brightness b = vfd.getBrightness();
```

| Constant | Level |
|---|---|
| `BRIGHTNESS_100` | 100 % (default) |
| `BRIGHTNESS_75` | 75 % |
| `BRIGHTNESS_50` | 50 % |
| `BRIGHTNESS_25` | 25 % |

The display can't report its brightness; `getBrightness()` returns the last
value set.

## Scrolling and direction

| Function | Description |
|---|---|
| `scrollDisplayLeft()` / `scrollDisplayRight()` | Shift both lines one place. Each 40-character line wraps onto itself. `home()` or `clear()` undoes the shift. |
| `moveCursorLeft()` / `moveCursorRight()` | Move the cursor one place without writing. |
| `leftToRight()` / `rightToLeft()` | Which way the cursor moves after each character. |
| `autoscroll()` / `noAutoscroll()` | With autoscroll on, the display shifts on each write so the cursor stays still on screen (ticker effect). |

## User-defined characters

```cpp
const uint8_t bell[8] = { 0x04, 0x0E, 0x0E, 0x0E, 0x1F, 0x00, 0x04, 0x00 };
vfd.createChar(0, bell);
vfd.setCursor(0, 0);   // createChar leaves the cursor at (0,0)
vfd.write((uint8_t)0); // cast needed: write(0) is ambiguous in C++
```

| Function | Description |
|---|---|
| `createChar(slot, rows[8])` | Define character 0–7. `rows[0..6]` = 5×7 dots (bit 4 = leftmost); `rows[7]` non-zero = underline. Characters already on screen update immediately. |
| `createChar_P(slot, rows)` | Same, with `rows` in PROGMEM. |

Codes 8–15 show the same characters as 0–7.

## Reading back (R/W wired)

These return 0 when reading isn't possible (`canRead()` is false).

| Function | Description |
|---|---|
| `canRead()` | `true` if `begin()` found a working busy flag. |
| `isBusy()` | `true` while an instruction is executing. |
| `readAddress()` | Current address counter (DD RAM: line 1 = 0x00–0x27, line 2 = 0x40–0x67). |
| `readChar(col, row)` | Character code at a position. Moves the cursor. |
| `readCharRow(slot, row)` | One row of a user character. Leaves the cursor at (0,0). |
| `readData()` | Raw data read at the current address. |
| `readStatus()` | Raw busy flag (bit 7) + address (bits 0–6). |

## Low level

| Function | Description |
|---|---|
| `command(value)` | Send any raw instruction. Constants such as `CU40025VFD::CMD_SHIFT` are provided. |
| `waitReady()` | Wait until the display can accept the next instruction. |

Constants: `COLS` (40), `ROWS` (2), `LINE1_ADDR` (0x00), `LINE2_ADDR`
(0x40), `CMD_*` instruction codes, and their flag bits (`ENTRY_INCREMENT`,
`DISPLAY_ON`, `SHIFT_RIGHT`, …). See [instruction-set.md](instruction-set.md).

## Porting from LiquidCrystal

| LiquidCrystal | CU40025VFD |
|---|---|
| `LiquidCrystal lcd(rs, en, d4, d5, d6, d7)` | `CU40025VFD vfd(rs, CU40025VFD::NO_PIN, en, d4, d5, d6, d7)` |
| `lcd.begin(40, 2)` | `vfd.begin()` (size is fixed) |
| everything else | same name |
| — | `setBrightness()`, `readChar()`, `readAddress()`, … (extras) |

## Performance

The library uses `digitalWrite()`/`digitalRead()` so that it works on any
board. On a 16 MHz AVR, a character takes roughly 40–60 µs, which is plenty
for text. If you need faster full-screen animation, the bus timing (666 ns
cycle) leaves plenty of headroom for direct port writes.
