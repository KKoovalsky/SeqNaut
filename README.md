# SeqnautDemo

## TODO

### Hardware

#### Analog Front-End (AFE)

- [ ] **Fix input jack soldering** — tip and ring are soldered the wrong way around, needs to be reflowed and swapped.
- [ ] **Add Rbias (10kΩ) between VBIAS node and IN+** — currently Cbias (47µF) has one leg lifted as a workaround because it was shunting the AC signal to GND (~3Ω at 1kHz). The proper fix is to isolate IN+ from the VBIAS node via a 10kΩ series resistor (Rbias), so Cbias stabilizes the DC midpoint without loading the signal path. Schematic note added. Needs to be soldered on the prototype.
