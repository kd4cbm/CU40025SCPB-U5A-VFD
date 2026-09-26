# Hardware

## Identifying the module

- The label or silkscreen reads **CU40025SCPB-U5A**, and the board is
  marked Noritake / Itron (Noritake Itron Corp., Japan).
- The glass tube shows 2 rows × 40 characters. When lit, the glow is green.
- It needs an external high-voltage rail (pins 17/18), which the catalogue
  CU40025SCPB-W6J makes on board with a DC/DC converter.
- Salvaged units come from the AT&T / Lucent / Avaya 8434DX telephone (see
  [8434DX-origin.md](8434DX-origin.md)).

### U5A compared with the W6J

| | CU40025SCPB-W6J (catalogue) | CU40025SCPB-U5A (OEM) |
|---|---|---|
| Display | 2 × 40, 5 × 7 + underline, green | Same |
| Controller / instruction set | HD44780-style + brightness | Same (verified) |
| Power | Single +5 V, on-board DC/DC | **+5 V and ≥ +18 V** (split supply) |
| Connector | 14 pins | 18 pins; 1–14 match the W6J |
| Datasheet | DS-805-0000-02 | None published |

## Connector

| Pin | Signal | Notes |
|---:|---|---|
| 1 | GND | Logic ground |
| 2 | +5 V | Logic supply |
| 3 | NC | No connection (per datasheet). Leave open. |
| 4 | RS | 0 = instruction register, 1 = data register |
| 5 | R/W | 1 = read, 0 = write |
| 6 | E | Enable strobe |
| 7–14 | DB0–DB7 | Data bus. DB7 is the busy flag during a status read. |
| 15 | NC | No connection. Leave open. |
| 16 | NC | No connection. Leave open. |
| 17 | GND | High-voltage supply ground |
| 18 | +18 V min | VFD drive supply |

Pin 1 is normally marked on the PCB (a square pad, or a "1" in the
silkscreen). **Check pin 1 before you apply power:** reversing a
connector puts the supply on the data bus.

## Electrical characteristics of the logic interface

From the W6J datasheet (same controller). All values at VCC = 5.0 V:

| Signal | Logic high (VIH) | Logic low (VIL) |
|---|---|---|
| DB0–DB7 | ≥ 2.2 V | ≤ 0.6 V |
| RS, R/W, E | ≥ 0.7 × VCC (3.5 V) | ≤ 0.3 × VCC (1.5 V) |

| Output (reads) | Value |
|---|---|
| VOH | ≥ VCC − 0.5 V at −1.6 mA |
| VOL | ≤ 0.4 V at 1.6 mA |

A 3.3 V board can drive the data lines directly (2.2 V threshold), but **not
the control lines** (they need 3.5 V). Use a 5 V-output buffer such as a
74HCT245 or 74AHCT125 for RS, R/W and E, or for everything. When reading,
the display drives the data lines at 5 V, which will damage a 3.3 V-only
input. The easy way out is to tie R/W to GND and never read.

## Bus modes

### M68 mode (default, used by this library)

Signals: RS, R/W, E. For a write, set RS and R/W, put the data on the bus,
then pulse E high; the display latches on the falling edge. For a read, set
R/W high and pulse E; the data is valid while E is high.

The datasheet cycle time is **666 ns**, and every instruction except Display
Clear (1.8 ms maximum) completes within one cycle. The Arduino library's
`digitalWrite()` timing is far slower than that, so no extra delays are
needed on AVR boards.

### i80 mode (jumper JP2)

On the W6J, shorting jumper **JP2** switches the bus to Intel 8080 style:
pin 5 becomes **WR** and pin 6 becomes **RD** (both active low). The factory
default is JP2 open (M68). *The i80 mode hasn't been tested on the U5A*, and
this library doesn't support it.

### 4-bit mode

Only DB4–DB7 are used. Each byte is sent as two nibbles, high nibble first,
and no busy check is needed between the two halves. Reads also come back as
two nibbles. Selected by the Function Set command (IF = 0); the library does
this when you use the 7-argument constructor.

## Reset

Pin 3 is **not connected** (per the datasheet). Leave it open. There is no
reset line to drive, and you don't need one: `begin()` fully reinitializes
the display by command.

For reference, the W6J datasheet describes a factory jumper option: shorting
**JP4 pins 1-2** turns pin 3 into an active-low reset input, and shorting
**JP4 pins 2-3** for more than 10 µs resets the module. That option isn't
fitted as standard. Whether the U5A even has JP4 hasn't been checked.

## Self-test (CN1)

The W6J has a 3-pin factory header, **CN1**. Shorting CN1 pins 2 and 3 at
power-up runs a built-in test that shows a checker pattern on every
character. Noritake marks CN1 as factory-use-only and possibly removed in
later builds. Whether the U5A has it is unverified. Test 5 of
`FullFeatureTest` shows the same kind of pattern from software.

Noritake also suggests about **2 hours of running in test mode** to settle
the brightness of a display that hasn't been powered for more than 2
months. That's worth knowing for surplus units that have been in storage.

## Optical and mechanical

From the W6J datasheet (the glass and layout are expected to be the same):

| | |
|---|---|
| Display area | 138.8 × 11.5 mm |
| Character size | 2.3 × 4.7 mm |
| Character pitch | 3.5 mm |
| Line pitch | 6.1 mm |
| Dot size / pitch | 0.38 × 0.5 mm / 0.48 × 0.7 mm |
| Luminance | ≥ 350 cd/m² |
| Colour | Green |
| Operating temperature | −40 to +85 °C |

The U5A's board outline and mounting holes may differ from the W6J's.
Measure your own unit before designing a panel cut-out.

## Handling

From Noritake's precautions for this module family:

- **ESD:** the board uses MOS LSI. Use anti-static handling and grounded tools.
- **Glass:** the tube is soda-lime glass. Avoid shocks over 100 G, thermal
  shocks over 10 °C/minute, and knocks, **especially to the exhaust pipe**
  (the small sealed glass nub on the tube).
- **Mounting:** don't press on the glass. Leave a small gap between the glass
  and any front panel. Twisting or warping the board can crack the glass
  around its lead pins.
- **Cables:** keep the data cable under **300 mm** to avoid noise problems.
- **Hot-plugging:** don't connect or disconnect the display while it is
  powered.
- **Burn-in:** showing the same static text for more than 5 hours a day can
  wear the phosphor unevenly. Blank the display or move the content when
  idle.
- **After power-off:** wait more than 1 minute for the capacitors to
  discharge before laying the board on anything conductive.
