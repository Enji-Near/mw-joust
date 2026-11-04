# Parts

Official datasheets exist only for the driver chip and the Arduino boards. The two motors are marketplace parts: no manufacturer datasheet was found, so their only source is the seller listing.

## DRV8833 driver module (Podazz, 8 pcs)

Datasheet: [TI DRV8833, SLVSAR1E, Rev. E](datasheets/TI_DRV8833_SLVSAR1E.pdf) ([source](https://www.ti.com/product/DRV8833))

| Spec | Value | Source |
| --- | --- | --- |
| Motor supply | 2.7–10.8 V operating, 11.8 V absolute max | TI §6.1, §6.3 |
| Current per bridge | 1.5 A RMS (PWP, RTY packages); 500 mA RMS (PW package) | TI |
| Bridges per chip | 2 (a stepper needs both, a DC motor needs one) | TI |
| Sleep input | Internal pulldown: chip sleeps unless driven high | TI pin table |
| Fault output | Open drain; low on overcurrent, overtemperature, undervoltage | TI §7.3.5 |
| Faults recover | Automatically | TI §7.3.5 |

The seller's 1.5 A and "single H-bridge" claims don't match or aren't confirmed by TI: the chip has two bridges, and 1.5 A doesn't apply if these modules use the PW package. That doesn't matter for this bench test, but check the package before relying on these for higher loads.

## N20 encoder gearmotor (MECCANIXITY, ASIN B0F8NJF9GB)

Source: seller listing only.

| Spec | Value |
| --- | --- |
| Type | Brushed DC gearmotor with encoder |
| Voltage | DC 6 V |
| Speed | 50 RPM |
| Ratio | 1:298 |
| Current | 0.15 A |
| Output shaft | 3 mm dia, 10 mm long, D-type |
| Size | 40.5 × 12 × 10 mm |

**Not listed:** encoder type, resolution, voltage, stall current.

Wire labels, read from the motors in connector order: red M2, black VCC, yellow C2, green C1, blue GND, white M1.

The encoder is incremental: it counts movement and has no absolute position. The count is lost at power-off, so anything needing absolute position must home against a reference such as a limit switch.

## 8 mm micro stepper (Abovehill, ASIN B08346RFVZ)

Source: seller listing only.

| Spec | Value |
| --- | --- |
| Type | 2-phase, 4-wire, no gearbox |
| Voltage | 5–6 V |
| Phase resistance | 39.2 Ω |
| Drive current | About 0.12 A |
| Body | 8 mm dia, 8.2 mm high |
| Output shaft | 1.5 mm dia; 4 mm (long) or 3.5 mm (short) |
| Pinion | Copper, 9 teeth, 0.2 modulus, 2.2 mm OD |

| Shaft | A+ | A− | B+ | B− |
| --- | --- | --- | --- | --- |
| Long | Blue | Black | Red | White |
| Short | Purple | Yellow (see note) | Orange | Green |

- **Colour note:** the listing labels both yellow and green as B− on the short-shaft version, which can't be right. Yellow as A− is an inference, so confirm the pairs with a meter.
- **Step angle isn't listed.** 18° is assumed because similar 8 mm motors use it. Confirm with the `t 1 20` bench command.

## Arduino Mega 2560 Rev3 (A000067): bench test board

Datasheet: [Arduino user manual](datasheets/Arduino_Mega_2560_Rev3_A000067.pdf) ([source](https://docs.arduino.cc/resources/datasheets/A000067-datasheet.pdf))

| Spec | Value | Source |
| --- | --- | --- |
| VIN | 7–12 V | §2.1 |
| 5 V regulator | SPX1117 linear, 750 mA | §3.4 |
| EEPROM | 4 KB | Features |
| External interrupts | D2, D3, D18–D21 | Core `pins_arduino.h` |
| PWM | D2–D13, D44–D46 | Core `pins_arduino.h` |
| Reset flags | Cleared by the bootloader before the sketch runs | `stk500boot.c` |

## Arduino Nano Every (ABX00028): first choice, not used

Datasheet: [Arduino user manual](datasheets/Arduino_Nano_Every_ABX00028.pdf) ([source](https://docs.arduino.cc/resources/datasheets/ABX00028-datasheet.pdf))

| Spec | Value | Source |
| --- | --- | --- |
| VIN | Up to 21 V | p.2 |
| +5 V rail | 1 A DC-DC, 950 mA for user circuits | §3.4 |
| USB to VIN | Through a Schottky diode | §3.4 |
| GPIO | A0–A7 usable as GPIO | §6.2 |
| PWM | D3, D5, D6, D9, D10 | §6.2 |
| External interrupts | All general-purpose pins | p.2 |
