# Standalone demo

`CU40025_Demo.ino` is the original single-file feature test. It has its own
built-in driver, so it needs no library. Copy it into a folder named
`CU40025_Demo` and open it in the Arduino IDE.

Wiring: RS=2, R/W=3, E=4, DB0-DB7=5-12 (see the main README).

For new projects, use the `CU40025VFD` library and its `FullFeatureTest`
example instead. They run the same tests.
