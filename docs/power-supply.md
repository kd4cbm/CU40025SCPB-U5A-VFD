# Power supply

The CU40025SCPB-U5A needs a **split power supply**. It will not work from a
single 5 V rail the way most character displays do.

| Rail | + pin | GND pin | Voltage | Purpose |
|---|---|---|---|---|
| Logic | 2 | 1 | +5.0 V (4.75–5.25 V) | Controller, character generator, bus interface |
| VFD drive | 18 | 17 | **+18 V minimum** | Drive voltage for the vacuum fluorescent tube |

## Why two supplies?

A VFD is a vacuum tube. Its phosphor-coated anodes and control grids need
tens of volts, much more than the 5 V logic uses. The catalogue version,
the **CU40025SCPB-W6J**, has an on-board DC/DC converter that makes those
voltages from +5 V, so it only needs a single supply (330 mA typical, 430 mA
maximum, with all dots lit).

The **U5A** was built for the Avaya 8434DX telephone. The phone already had
its own power circuitry, so this OEM version leaves the converter off and
takes the high-voltage rail directly on pins 17/18. What the display does
with that rail internally (grid and anode drive, filament) hasn't been traced
yet. Treat pin 18 simply as a supply input that needs at least 18 V.

## Known and unknown values

| Parameter | Value | Source |
|---|---|---|
| Logic supply | 4.75 – 5.25 V | W6J datasheet (same controller) |
| VFD drive supply, minimum | 18 V | From the author's working U5A setup |
| VFD drive supply, maximum | **unknown** | Not yet characterized; stay close to 18 V |
| Logic current | **unknown** | Should be well below the W6J's 330 mA, because the W6J figure includes its DC/DC converter |
| VFD drive current | **unknown** | Worst case is FullFeatureTest test 5 (all dots) |
| Display-off current | **unknown** | The W6J draws 8 mA typical with the display off |

**Please measure and share.** Put a multimeter in series with each rail, run
`FullFeatureTest`, and note the current during test 5 (all dots on, 100 %
brightness) and test 7 (display off). Open an issue with the results.

## Ways to make the 18 V rail

### 1. Boost converter from 5 V (recommended)

A small adjustable boost module lets you run the whole project from one 5 V
supply, such as USB or a 5 V wall adapter:

```
               +5V ─────┬──────────────────────── Display pin 2
                        │
 5 V supply             ├──► Boost IN+   Boost OUT+ ──► Display pin 18
 (USB, wall             │    (set to 18 V)
  adapter, etc.)        │
               GND ─────┴──┬─ Boost IN−  Boost OUT− ──► Display pin 17
                           │
                           ├──────────────────────── Display pin 1
                           └──────────────────────── Arduino GND
```

- Common hobby boost modules (for example MT3608-based boards, rated up to
  about 28 V out) can do this.
- **Set the output voltage before you connect the display.** Many modules ship
  set to their maximum. Adjust the trimmer with only a multimeter on the
  output, then connect the display.
- Keep the boost module's switching noise away from the data lines. Keep
  its wiring short, and add a 100 µF electrolytic plus a 100 nF ceramic across
  pins 17/18 at the display.
- Budget the 5 V side for the boost converter's input current. Its input
  current is roughly (18 V × I₁₈) ÷ (5 V × efficiency).

### 2. Bench power supply

Useful for first bring-up, because you can set a current limit. Start at
18 V with a modest current limit (a few hundred mA) and watch the current
as you run the tests.

### 3. Existing higher-voltage rail

If your project already has a 24 V rail, for example, use a buck regulator
to bring it down to 18 V. **Don't feed the display 24 V directly**: the
maximum voltage is unknown.

> ⚠️ **Don't use the 8434DX phone's power adapter directly.** The phone
> runs from about 48 V (from the phone system, or a local Avaya 1151-series
> adapter) and makes its own internal rails. 48 V on pin 18 is far above the
> known operating point.

## 5 V supply notes

- Use a regulated supply that rises quickly. The W6J datasheet warns that a
  slow-rising 5 V (> 50 ms) can make the power-on reset fail. The library
  always initializes by command, so this is covered in software, but a clean
  supply is still best.
- Powering the logic rail from an Arduino's 5 V pin on USB is fine for the
  logic alone. If the 18 V boost converter also runs from the same USB 5 V,
  watch the total: USB ports are commonly limited to 500 mA.

## Sequencing and hot-plugging

The W6J datasheet warns:

- Don't connect or disconnect the data cable or the power connector while
  power is applied.
- Removing primary power while logic signals are still applied may damage
  the input circuitry.

For the split-supply U5A, a sensible practice (general VFD practice, not
from a U5A document) is:

1. Bring up +5 V first, or both rails together.
2. Start driving the bus only once the display is powered. The library
   waits 100 ms in `begin()`.
3. When powering down, remove 18 V first, or both together. Don't leave the
   Arduino driving the bus into an unpowered display.

The simplest way to satisfy all of this is to run the Arduino, the 5 V rail
and the boost converter from one supply with one switch.

## Grounding

**All grounds must be common:** display pin 1, display pin 17, the Arduino
GND, and the negative side of every supply. Without a shared ground the bus
signals have no reference and the display will behave erratically, or not
respond at all.

## Safety

- 18 V isn't dangerous to touch, but the tube is thin glass under vacuum. See
  the handling notes in [hardware.md](hardware.md).
- The W6J datasheet notes that the module's large capacitors need **more than
  1 minute to discharge** after power-off. Don't lay the board on a
  conductive surface straight after switching off.
