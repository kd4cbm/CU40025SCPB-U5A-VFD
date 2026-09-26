# Troubleshooting

Start with the `FullFeatureTest` example and open the Serial Monitor at
115200 baud. The first line tells you whether the bus read-back works, and
that alone separates wiring problems from power problems.

## Nothing lights up at all

| Serial says | Likely cause |
|---|---|
| `Busy flag / read-back: working` | The logic side is fine. The **18 V rail** (pins 17/18) is probably missing, too low, or its ground isn't shared. Measure between pins 17 and 18 with the display connected. |
| `NOT working (fixed delays)` | The logic side isn't responding: check +5 V on pin 2, GND on pin 1, and the common ground with the Arduino. Then check the wiring of RS, R/W and E. Pin 1 may be reversed. |

Also check:
- The 18 V supply is at least 18 V **under load**. A boost module that
  sags when every dot is lit (test 5) will show up as dimming or flicker.
- The display hasn't been turned off by software (`noDisplay()`).

## Garbage characters, or text in the wrong places

- **One data line wrong or swapped:** characters come out consistently
  wrong, such as the wrong letters or a pattern shifted by a power of two.
  Test 3 (character font) makes this easy to spot, and test 1 reports
  which DD RAM cells failed. Compare the expected and received values in
  the serial log; the bits that differ tell you which line is at fault.
- **Floating or noisy E line:** random characters or random commands. Keep
  wires short (under 300 mm), and give the display and the Arduino a solid
  common ground.
- **Wrong constructor:** 4-bit wiring with the 8-bit constructor (or the
  reverse) gives garbage straight away.

## Works after upload, garbage after resetting only the Arduino

This happens if the display is in 4-bit mode and the Arduino resets halfway
through a two-nibble transfer. `begin()` sends the standard resync sequence
(three 8-bit Function Set nibbles), which recovers from this, so if you see
it, make sure `begin()` is called on every start.

## Read-back test fails but the text looks right

- **R/W not wired, or tied to GND:** expected. Use `NO_PIN` in the
  constructor.
- **3.3 V board without level shifting:** the display may not see the
  control lines as logic high (they need 3.5 V). See
  [hardware.md](hardware.md#electrical-characteristics-of-the-logic-interface).
- Individual DD RAM mismatches point to a bad data line, as above.

## CG RAM reads return wrong bytes

**This is a module quirk.** The CG RAM (user characters) is stored
correctly, but **several CG RAM reads in a row skip addresses**: the address
counter sometimes advances by two. In testing, reading 64 bytes in a row
returned bytes from addresses 0, 2, 4, 6, 7, 8, 10, … Sequential DD RAM
reads are not affected.

The datasheet says to set the RAM address immediately before a Read Data
anyway. The library's `readCharRow()` does that for every byte. If you
write your own low-level code, do the same:

```cpp
vfd.command(CU40025VFD::CMD_CGRAM | (slot << 3) | row);
uint8_t bits = vfd.readData();
```

## User characters show the wrong shape

- Rows are top to bottom, and **bit 4 is the leftmost dot**. Bits 7–5 are
  ignored.
- The 8th byte is only the underline. There is no 8th dot row.
- `createChar()` leaves the cursor at (0,0) in DD RAM. Call `setCursor()`
  before printing.
- `vfd.write(0)` won't compile (it's ambiguous in C++). Use
  `vfd.write((uint8_t)0)`, or use slot 8, which shows the same character.

## Brightness doesn't change

The brightness byte must directly follow a Function Set instruction.
`setBrightness()` handles that. If you send raw commands, don't put any
other instruction between the two.

## Dim or patchy display

- The 18 V rail is low or sagging. Measure it during test 5.
- **Long storage:** Noritake recommends about 2 hours of running to
  stabilize brightness if a display hasn't been powered for more than 2
  months. Running `FullFeatureTest` for a couple of hours does this nicely.
- **Burn-in:** characters that were shown for years in the phone (such as a
  clock position or the softkey labels) can be permanently dimmer. There's
  no fix. Check for it before buying, if the listing has photos of the
  display lit.

## Characters in the upper half (80h–FFh) look unfamiliar

The built-in font (codes 10h–FFh) comes from the display's character
generator. The W6J datasheet includes a font table, but the U5A is an OEM
part whose font may differ. Test 3 shows all 256 codes. Photograph it
against the W6J table and share any differences.

## Still stuck?

Open an issue with:
- Board and wiring (constructor line)
- Voltages measured on pins 2 and 18, under load
- The serial log from `FullFeatureTest`
- A photo of the display, if something lights up
