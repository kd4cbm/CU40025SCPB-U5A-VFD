# Origin: the AT&T / Lucent / Avaya 8434DX telephone

The CU40025SCPB-U5A appears to be an OEM display that Noritake Itron made
for the **8434DX** multi-line desk telephone. The 8434DX was sold under the
AT&T, Lucent Technologies and later Avaya names, as the brand passed from
AT&T to Lucent (1996) and then to Avaya (2000). That is why these displays
appear on eBay in quantity: large numbers of 8434DX phones have been retired
from offices and scrapped.

## About the phone

| | |
|---|---|
| Type | Digital (DCP) multi-line voice terminal for Definity-family business phone systems (PBXs) |
| Model numbers | Sold as the 3236 series (e.g. 3236-06, 3236-08, 3236-10) under various comcodes |
| Display | 2-line × 40-character vacuum fluorescent display: this module |
| Buttons | 34 call-appearance/feature buttons, 5 softkeys under the display (15 softkey features), 9 fixed feature keys (Hold, Conference, Transfer, Drop, Volume, Mute, Speaker, Test, Shift) |
| Other | Built-in speakerphone; 2-wire or 4-wire DCP line; optional 7400B+/8400B+ data modules |
| Power | Needs auxiliary power for the display, from the phone system or a local Avaya **1151-series** adapter (about 48 V DC) |

The phone's need for auxiliary power fits the U5A's design. The 8434DX
takes about 48 V and makes its own internal rails, so its display didn't
need the on-board DC/DC converter of the catalogue W6J. The phone supplies
the display's high-voltage rail directly.

## Getting the display out of a phone

The 8434DX's internal construction hasn't been documented here yet. General
advice for recovering the display:

- **Unplug the phone** from the phone system and the aux power adapter, then
  wait a few minutes for its capacitors to discharge.
- Handle the display by the PCB edges, not the glass, and never by the
  **exhaust pipe** (the small sealed glass nub on the tube).
- Note or photograph the cable and connector orientation before unplugging,
  so you can find pin 1.
- If you can, keep the phone's mating connector or cable. It makes wiring
  much easier.

Photos and notes on the teardown (screw locations, connector type, the
cable to the main board)
would be very welcome as a contribution.

## If you have a working 8434DX

The phone's main board is a live reference for the unknowns in this
project:

- Measure the voltage the phone puts on **pin 18** during normal
  operation. That is the real design value, which may be more than the 18 V
  minimum.
- Measure the current on each rail.

## Sources

- Avaya, *8434DX Digital Terminal* product page:
  <https://support.avaya.com/elmodocs2/s8700/library/guide/8434DX.html>
- Twacomm, *Lucent 3236-08 8434DX Display Phone*:
  <https://twacomm.com/products/lucent-3236-08-8434dx-display-phone>
- Main Resource, *Avaya Definity 8434DX Speaker Display Phone with Power
  Supply* (sold with the 1151 adapter):
  <https://mainresource.com/products/avaya-definity-8434dx-speaker-display-phone-with-power-supply-white-refurbished>
- Start Tech, *Avaya 1151A1 power supply for the 8434DX*:
  <https://www.startechtel.com/avaya-1151a1-8434dx-power-supply-refurbished-p/ava1151a1-ref.htm>
