# CU40025SCPB-U5A VFD — Arduino driver and builder's guide

An Arduino library and hands-on documentation for the **Noritake Itron
CU40025SCPB-U5A**, a green 2-line × 40-character vacuum fluorescent display
(VFD). The module was an OEM part in the **AT&T / Lucent / Avaya 8434DX**
desk telephone, and it shows up regularly on eBay as surplus or salvage.

There is no public datasheet for the U5A. It is a variant of the
CU40025SCPB-W6J, which *is* documented (Noritake spec DS-805-0000-02), and it
uses the same controller and instruction set. The big difference is power:
**the U5A has no on-board DC/DC converter and needs a second supply of at
least 18 V** alongside the usual +5 V. Everything here was worked out from the
W6J datasheet and then verified on a real U5A.

> **Quick start:** wire it up as shown below, give it **+5 V on pin 2** and
> **at least +18 V on pin 18** (grounds on pins 1 and 17), install the library,
> and run `File → Examples → CU40025VFD → HelloWorld`.

---

## At a glance

| | |
|---|---|
| Manufacturer | Noritake Itron (Japan) |
| Part number | CU40025SCPB-U5A (OEM variant of CU40025SCPB-W6J) |
| Found in | AT&T / Lucent / Avaya 8434DX telephone ([more](docs/8434DX-origin.md)) |
| Display | 2 lines × 40 characters, green VFD |
| Character cell | 5 × 7 dots plus a full-width underline dot |
| User-defined characters | 8, in CG RAM (codes 0–7, mirrored at 8–15) |
| Controller | HD44780-compatible instruction set, plus 4-level brightness |
| Interface | Parallel, 8-bit or 4-bit, M68 style (RS, R/W, E), 5 V logic |
| Brightness | 100 / 75 / 50 / 25 % by command |
| Power | **Split supply:** +5 V logic (pin 2) **and** ≥ +18 V VFD drive (pin 18) |

## Connector pinout

Pins are numbered on the PCB. Pins 1–14 match the W6J's 14-pin connector.
Pins 15–18 are specific to the U5A.

| Pin | Signal | Direction | Notes |
|---:|---|---|---|
| 1 | GND | — | Logic ground |
| 2 | +5 V | power in | Logic supply, 4.75–5.25 V |
| 3 | NC | — | No connection (per datasheet). Leave open. |
| 4 | RS | in | Register select: 0 = instruction, 1 = data |
| 5 | R/W | in | 1 = read, 0 = write. Tie to GND if you never read. |
| 6 | E | in | Enable strobe; data is latched on the falling edge |
| 7 | DB0 | in/out | Data bit 0 (unused in 4-bit mode) |
| 8 | DB1 | in/out | Data bit 1 (unused in 4-bit mode) |
| 9 | DB2 | in/out | Data bit 2 (unused in 4-bit mode) |
| 10 | DB3 | in/out | Data bit 3 (unused in 4-bit mode) |
| 11 | DB4 | in/out | Data bit 4 |
| 12 | DB5 | in/out | Data bit 5 |
| 13 | DB6 | in/out | Data bit 6 |
| 14 | DB7 | in/out | Data bit 7; also the busy flag when reading status |
| 15 | NC | — | No connection. Leave open. |
| 16 | NC | — | No connection. Leave open. |
| 17 | GND | — | High-voltage supply ground |
| 18 | +18 V (minimum) | power in | VFD drive supply |

Full details, jumpers and handling notes: [docs/hardware.md](docs/hardware.md).

## Power — read this first

The U5A needs **two supplies**, and it will not light without the second one:

| Rail | Pins | Voltage | Current |
|---|---|---|---|
| Logic | 2 (+), 1 (GND) | +5 V ±5 % | *not yet measured on the U5A* |
| VFD drive | 18 (+), 17 (GND) | **18 V minimum** | *not yet measured on the U5A* |

- **Join the grounds:** pins 1 and 17, the Arduino GND and both supply
  grounds must all be connected.
- **Start at 18 V.** The maximum safe voltage has not been characterized, so
  don't go much above it.
- **Likely symptom of a missing 18 V rail:** the logic side still works
  (read-back tests pass), but nothing lights up.
- A small adjustable boost converter running from the same 5 V supply is the
  simplest way to make the 18 V rail.

Supply options, sequencing, budgeting and safety: [docs/power-supply.md](docs/power-supply.md).

## Wiring to an Arduino Uno / Mega (8-bit)

This is the wiring used by the examples and tested on an Arduino Uno:

| Arduino pin | 2 | 3 | 4 | 5 | 6 | 7 | 8 | 9 | 10 | 11 | 12 |
|---|---|---|---|---|---|---|---|---|---|---|---|
| Signal | RS | R/W | E | DB0 | DB1 | DB2 | DB3 | DB4 | DB5 | DB6 | DB7 |
| Display pin | 4 | 5 | 6 | 7 | 8 | 9 | 10 | 11 | 12 | 13 | 14 |

Any digital pins will do; pass them to the constructor. To save pins:

- **4-bit mode:** connect only DB4–DB7 (7 wires in total). See the
  `FourBitMode` example.
- **Write-only:** tie R/W (pin 5) to GND and pass `CU40025VFD::NO_PIN`. Writes
  use fixed delays instead of polling the busy flag, and read-back is not
  available.
- **4-bit + write-only:** 6 wires in total (RS, E, DB4–DB7).

**3.3 V boards (ESP32, RP2040, STM32, etc.):** the display is a 5 V part. RS, R/W and E
need at least 3.5 V for a logic high, so use a level shifter such as a 74HCT245
or 74AHCT125. If you read from the display, its 5 V outputs must also be
shifted down. The simplest way to avoid that is to wire it write-only.

## Installing the library

1. Download this repository as a ZIP (**Code → Download ZIP**).
2. In the Arduino IDE: **Sketch → Include Library → Add .ZIP Library…** and
   pick the ZIP.

Or clone it straight into your Arduino `libraries` folder:

```bash
git clone https://github.com/kd4cbm/CU40025SCPB-U5A-VFD.git
```

## Minimal sketch

```cpp
#include <CU40025VFD.h>

//             RS R/W E  DB0 DB1 DB2 DB3 DB4 DB5 DB6 DB7
CU40025VFD vfd(2, 3,  4, 5,  6,  7,  8,  9,  10, 11, 12);

void setup() {
  vfd.begin();                                   // true if read-back works
  vfd.print("Hello, world!");
  vfd.setCursor(0, 1);                           // column 0, line 2
  vfd.print("CU40025SCPB-U5A");
  vfd.setBrightness(CU40025VFD::BRIGHTNESS_50);  // 100 / 75 / 50 / 25 %
}

void loop() {}
```

The API follows Arduino's `LiquidCrystal` (`clear`, `setCursor`, `cursor`,
`blink`, `scrollDisplayLeft`, `createChar`, …), so most LCD code ports with
little more than a change of class name. Full reference:
[docs/library-reference.md](docs/library-reference.md).

## Examples

| Example | What it shows |
|---|---|
| [HelloWorld](examples/HelloWorld/HelloWorld.ino) | Minimum working sketch with an uptime counter |
| [FourBitMode](examples/FourBitMode/FourBitMode.ino) | 4-bit bus, read-back over 4 bits, brightness cycling |
| [FullFeatureTest](examples/FullFeatureTest/FullFeatureTest.ino) | Every documented feature, each labelled on screen; loops forever. The best way to check a newly bought display. |

[`extras/standalone-demo`](extras/standalone-demo/CU40025_Demo.ino) contains
the original single-file test sketch, which doesn't need the library. It's
handy if you only want to check that a display works.

### What FullFeatureTest checks

| # | Test | You should see |
|---|---|---|
| 1 | Bus read-back | A pattern fills the screen, then PASS/FAIL for DD RAM, CG RAM and the address counter |
| 2 | DD RAM addressing | Column ruler, then a block walking through all 80 cells |
| 3 | Character font | All 256 character codes over 4 pages |
| 4 | User fonts | 8 icons (the last one underlined), an animated Pac-Man, a smooth bar graph |
| 5 | All dots / checker | Every dot lit, then alternating checkerboards (maximum current draw) |
| 6 | Brightness | 100 → 75 → 50 → 25 %, then a fade cycle |
| 7 | Display ON/OFF | Screen blanks, and the text comes back unchanged |
| 8 | Cursor modes | Underline cursor, blinking character (~1.1 Hz), both |
| 9 | Entry mode | Typing left-to-right, right-to-left, and ticker scrolling both ways |
| 10 | Shift / Home / Clear | Both lines scroll a full turn, Home undoes the shift, Clear blanks the screen |

Open the Serial Monitor at 115200 baud to follow the log.

## Known quirks

- **CG RAM reads skip addresses** when you read several bytes in a row. DD RAM
  reads are fine. The library sets the address before each CG RAM read, which
  is also what the datasheet asks for. Details in
  [docs/troubleshooting.md](docs/troubleshooting.md).
- **Burn-in:** VFD phosphor wears. Noritake warns that showing the same
  static text for more than about 5 hours a day can burn it in. Blank the
  display (`noDisplay()`) or move content around when idle.
- The DD RAM holds exactly 40 characters per line, so scrolling wraps each
  line around onto itself.

## What has been verified

Tested on a CU40025SCPB-U5A with an Arduino Uno (September 2026):

| Item | Status |
|---|---|
| Pins 1, 2, 4–14, 17, 18 as listed | ✅ Verified |
| 8-bit M68 bus, write and read | ✅ Verified |
| 4-bit M68 bus, write and read | ✅ Verified |
| Busy flag and address counter read | ✅ Verified |
| DD RAM read-back (80/80), CG RAM read-back (64/64) | ✅ Verified |
| All instructions (clear, home, entry mode, display/cursor/blink, shift, brightness, CG/DD RAM) | ✅ Verified, and checked by eye |
| Pin 3 | ✅ No connection (per datasheet) |
| Pins 15 and 16 | ✅ No connection |
| Supply currents; maximum voltage on pin 18 | ❓ Not yet measured |
| i80 bus mode (jumper JP2), CN1 self-test header | ❓ Not tested on the U5A |
| Differences between the U5A and W6J built-in fonts | ❓ Run test 3 and compare with the W6J datasheet |

If you can fill in any of the ❓ rows, please open an issue or a pull request.

## Documentation

- [Hardware: connector, jumpers, handling](docs/hardware.md)
- [Power supply: rails, options, sequencing](docs/power-supply.md)
- [Instruction set](docs/instruction-set.md)
- [Library reference](docs/library-reference.md)
- [Origin: the Avaya 8434DX telephone](docs/8434DX-origin.md)
- [Troubleshooting](docs/troubleshooting.md)

## References

- Noritake Itron, *Vacuum Fluorescent Display Module Specification,
  CU40025SCPB-W6J*, spec no. DS-805-0000-02 (Mar 2001, rev. Jul 2002). This
  repository doesn't include it, because it is Noritake's copyrighted document. Search
  for the part number or spec number to find a copy.

## License

MIT. See [LICENSE](LICENSE). "Noritake", "Itron", "AT&T", "Lucent" and "Avaya"
are trademarks of their respective owners. This project is not affiliated with
any of them.
