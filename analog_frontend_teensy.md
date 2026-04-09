# Analog Front-End Design for Teensy Audio Shield (GX-100 Input Tap)

## Overview

This document describes a simple, high-impedance analog front-end for
tapping an audio signal from a multi-effects processor (e.g., Boss
GX-100) and feeding it into a Teensy Audio Shield for analysis (not
primary audio path).

The design ensures: - Minimal loading of the original signal path - Safe
signal levels for ADC input - Proper biasing for single-supply op-amp
operation

------------------------------------------------------------------------

## Signal Chain

    Input Jack (TS)
       |
       +---------------------------> Output Jack (Thru)
       |
       +--> High-Z Tap --> AC Coupling --> Bias --> Buffer --> AC Coupling --> Attenuator --> Teensy Line-In

------------------------------------------------------------------------

## Components and Values

### 1. Input / Thru Connection

-   Direct connection:
    -   Tip IN → Tip OUT
    -   Sleeve IN → Sleeve OUT
-   Pulldown resistor:
    -   Rg = 1 MΩ (Tip to GND)

------------------------------------------------------------------------

### 2. Bias Network (Midpoint)

-   Rb1 = 1 MΩ (5V → VBIAS)
-   Rb2 = 1 MΩ (VBIAS → GND)
-   Cbias = 10 µF + 100 nF (VBIAS → GND)

Result: VBIAS ≈ 2.5 V

------------------------------------------------------------------------

### 3. Input AC Coupling

-   C1 = 470 nF

------------------------------------------------------------------------

### 4. Buffer (Op-Amp: MCP6021)

Configuration: Voltage follower

Connections: - VDD (pin 5) → +5 V - VSS (pin 2) → GND - VIN+ (pin 3) →
signal after C1 + VBIAS - VIN- (pin 4) → VOUT (pin 1) - VOUT (pin 1) →
output signal

Decoupling: - 100 nF + 4.7 µF between VDD and GND (close to IC)

------------------------------------------------------------------------

### 5. Output AC Coupling

-   C2 = 1 µF

------------------------------------------------------------------------

### 6. Attenuator (to Teensy Line-In)

-   R1 = 22 kΩ (series)
-   R2 = 10 kΩ (to GND)

Optional: - Add 1 kΩ in series before ADC for protection

------------------------------------------------------------------------

## Final Output Connection

    Buffer OUT → C2 → R1 → +----> LINEIN_L (Teensy Audio Shield)
                             |
                            R2
                             |
                            GND

------------------------------------------------------------------------

## Notes

-   This design assumes a mono signal. Duplicate for stereo if needed.
-   Do NOT feed DC bias into Teensy input --- AC coupling removes it.
-   MCP6021 is rail-to-rail and suitable for 5 V single-supply
    operation.
-   Teensy Audio Shield expects line-level signals (\~1 Vrms max).
-   Adjust attenuator values if clipping or low signal occurs.

------------------------------------------------------------------------

## Design Goals Achieved

-   High input impedance (minimal signal loading)
-   Safe signal levels for ADC
-   Clean and stable biasing
-   Simple and robust architecture

------------------------------------------------------------------------

## Optional Improvements

-   Add configurable attenuator (switchable resistor values)
-   Add input protection (diodes or clamp)
-   Add RC filtering for noise reduction
