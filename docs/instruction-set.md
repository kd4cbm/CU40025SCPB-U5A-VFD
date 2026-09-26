# Instruction set

The controller uses the familiar HD44780 instruction set, with one addition:
**brightness control**. This page summarizes the commands in the
CU40025SCPB-W6J datasheet (DS-805-0000-02), all of which were verified on a
U5A. For each command, the library function that sends it is listed.

## Summary

| Instruction | RS | R/W | Code (binary) | Hex | Time | Library |
|---|:-:|:-:|---|---|---|---|
| Display clear | 0 | 0 | `0000 0001` | 01 | 1.8 ms max | `clear()` |
| Cursor home | 0 | 0 | `0000 001*` | 02–03 | 666 ns | `home()` |
| Entry mode set | 0 | 0 | `0000 01 I/D S` | 04–07 | 666 ns | `leftToRight()`, `rightToLeft()`, `autoscroll()`, `noAutoscroll()` |
| Display on/off control | 0 | 0 | `0000 1 D C B` | 08–0F | 666 ns | `display()`, `cursor()`, `blink()`, `setDisplayControl()` |
| Cursor/display shift | 0 | 0 | `0001 S/C R/L **` | 10–1F | 666 ns | `scrollDisplayLeft/Right()`, `moveCursorLeft/Right()` |
| Function set | 0 | 0 | `001 IF ****` | 20–3F | 666 ns | `begin()`, `setBrightness()` |
| Brightness (data byte after Function set) | 1 | 0 | `**** **BR1 BR0` | 00–03 | 666 ns | `setBrightness()` |
| Set CG RAM address | 0 | 0 | `01 AAAAAA` | 40–7F | 666 ns | `createChar()`, `readCharRow()` |
| Set DD RAM address | 0 | 0 | `1 AAAAAAA` | 80–E7 | 666 ns | `setCursor()` |
| Read busy flag & address | 0 | 1 | `BF AAAAAAA` | — | 666 ns | `readStatus()`, `isBusy()`, `readAddress()` |
| Write data | 1 | 0 | data | 00–FF | 666 ns | `write()`, `print()` |
| Read data | 1 | 1 | data | — | 666 ns | `readData()`, `readChar()` |

`*` = don't care.

## Details

### Display clear (01h)
Fills all of DD RAM with 20h (space), sets the address counter to 0 in DD
RAM, removes any display shift, and sets the entry mode to increment. This
is the only slow instruction, taking up to 1.8 ms.

### Cursor home (02h)
Sets the address counter to 0 and removes any display shift. Text is left
unchanged.

### Entry mode set (04h–07h)

| I/D | S | After each write |
|:-:|:-:|---|
| 1 | 0 | Cursor moves right (normal) |
| 0 | 0 | Cursor moves left |
| 1 | 1 | Display shifts left and the cursor stays put on screen ("ticker") |
| 0 | 1 | Display shifts right and the cursor stays put on screen |

Both lines always shift together. Reads move the cursor but never shift the
display.

### Display on/off control (08h–0Fh)
- **D:** whole display on/off. DD RAM keeps its contents while off.
- **C:** underline cursor on/off.
- **B:** blinks the character at the cursor, alternating between the
  character and all dots on, at about 1.1 Hz and a 50 % duty cycle.

### Cursor/display shift (10h–1Fh)

| S/C | R/L | Effect |
|:-:|:-:|---|
| 0 | 0 | Cursor left |
| 0 | 1 | Cursor right |
| 1 | 0 | Display (and cursor) shift left |
| 1 | 1 | Display (and cursor) shift right |

DD RAM is not changed. Each line holds exactly 40 characters, so a shifted
line wraps around onto itself.

### Function set (20h–3Fh) and brightness
**IF = 1** selects the 8-bit bus and **IF = 0** the 4-bit bus. This must be
the first instruction after power-on.

The **next byte written with RS = 1** after Function set is taken as
brightness, not as a character:

| BR1 BR0 | Brightness |
|:-:|---|
| 00 | 100 % (default) |
| 01 | 75 % |
| 10 | 50 % |
| 11 | 25 % |

If an instruction (RS = 0) is written after Function set instead, brightness
is left unchanged. So to change brightness you always send Function set
(with the correct IF bit) followed immediately by the brightness byte.
`setBrightness()` does exactly that.

### Set CG RAM address (40h–7Fh)
Selects a 6-bit CG RAM address: bits 5–3 = character (0–7), bits 2–0 = row
(0–7). The address wraps from 3Fh back to 00h.

### Set DD RAM address (80h + address)

| Line | Addresses | Command bytes |
|---|---|---|
| 1 | 00h–27h | 80h–A7h |
| 2 | 40h–67h | C0h–E7h |

### Read busy flag and address
Bit 7 = busy flag (1 = busy), bits 6–0 = address counter. Reading this
doesn't disturb anything, so it is safe to poll.

### Write data / read data
Reads or writes the DD RAM or CG RAM, depending on which address was last
set, then moves the address counter according to the entry mode. **Set an
address immediately before reading.** On this module that is mandatory for
CG RAM (see [troubleshooting](troubleshooting.md#cg-ram-reads-return-wrong-bytes)).

## User-defined characters (CG RAM)

There are 8 characters × 8 bytes = 64 bytes of CG RAM. Character codes
**00h–07h** display them, and **08h–0Fh** display the same eight again.

Each character is 8 bytes, top row first:

```
byte  bit: 4 3 2 1 0        example: 'A' with underline
 0         . . . . .        0x0E   . # # # .
 1         . . . . .        0x11   # . . . #
 2         . . . . .        0x11   # . . . #
 3         5 x 7 dots       0x1F   # # # # #
 4         . . . . .        0x11   # . . . #
 5         . . . . .        0x11   # . . . #
 6         . . . . .        0x11   # . . . #
 7         underline (dot 36)  0x1F   ─────────
```

Bits 7–5 of each byte are ignored. In byte 7, the datasheet assigns the
single underline dot (dot 36, which spans the full width of the cell) to
**DB2**. The library writes 1Fh or 00h there, so any non-zero value turns
the underline on.

## Power-on state

Power-on reset (if the supply rises in under about 50 ms) sets: display
cleared, address 0, **display off**, cursor off, blink off, increment, no
shift, 8-bit bus, brightness 100 %. Because the power-on reset isn't
reliable, the library always initializes by command.
