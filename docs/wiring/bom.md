# Joust v2: bill of materials (parts v2 adds)

Draft 0.1, 2026-11-08. References match `kicad/joust-v2.kicad_sch`. "Have" = owner already has it.
Values come from `tasks/todo.md` H1-H3 and `tasks/plan.md` (ADR-007, ADR-011). Part numbers are not chosen; any part meeting the spec works.

## Capacitors

| Ref | Qty | Value | Spec | Where |
| --- | --- | --- | --- | --- |
| C1 | 1 | 330 uF | Aluminium electrolytic, low-ESR, 35 V (25 V minimum), radial | Supply input, before the relay. **Fitted** (todo H3, 2026-11-08) |
| C3, C9 | 2 | 100 uF | Aluminium electrolytic, low-ESR, 35 V (25 V minimum), radial | Shield motor terminal; DRV8871 VM. **Fitted** (todo H3) |
| C2, C4, C10 | 3 | 100 nF | Ceramic X7R, 50 V, leaded | Beside C1, C3, C9. **Fitted** (todo H3) |
| C5-C8, C11 | 5 | 100 nF | Ceramic X7R, 50 V, leaded (5 mm pitch) | Directly across each motor's terminals. **Fitted** (todo H1, 2026-11-08) |
| C12-C21 | 10 | 100 nF | Ceramic X7R, 50 V, leaded | Limit input filters, at the Mega pin |
| (optional) | 10 | 100 nF | Ceramic X7R, 50 V | Each motor terminal to the motor case |

- 100 nF still to fit: 10 (C12-C21), 20 with the optional case caps.
- Never fit an electrolytic across a motor (todo H1).
- Electrolytics: observe polarity, keep leads short.

## Resistors

| Ref | Qty | Value | Spec | Where |
| --- | --- | --- | --- | --- |
| R11, R13 ... R29 (odd) | 10 | 1 kOhm | 1/4 W, 5 % or better, metal film | Pull-up, switch node to 5 V |
| R12, R14 ... R30 (even) | 10 | 1 kOhm | same | Series, switch node to Mega pin |

- 20 x 1 kOhm required; buy a 50- or 100-pack.
- Pull-up current: 5 mA per closed switch, 25 mW per resistor. All 10 closed: 50 mA from the Mega 5 V (USB).

## Power and switching

| Ref | Qty | Part | Spec | Status |
| --- | --- | --- | --- | --- |
| PS1 | 1 | 12 V DC supply | Current rating TBD (open item 1); must exceed the measured full-show peak | Have (one of the v1 supplies) |
| F1 | 1 | Fuse, 3 A time-lag | Plus a holder rated above 3 A at 12 V; check fuse size (5x20 mm assumed, unverified) | Have (v1 fuse). Buy 5 spares |
| K1 | 1 | Motor-power relay module | Contacts above 3 A at 12 V DC; must stay OFF with input floating | **Fitted** (todo H3, 2026-11-08); single-channel board later if the placeholder is in |
| Q1 | 1 | TIP120 NPN Darlington, TO-220 (Digikey 497-2539-5-ND) | Low-side switch for the 12 V start-button LED, from D13 | Have |
| R1 | 1 | 1 kOhm, 1/4 W | Q1 base resistor, D13 to base | From the 1 kOhm pack |
| U3 | 1 | Adafruit DRV8871 breakout (product 3190) | VM 6.5-45 V; current limit about 2.1 A with the 30 kOhm ILIM (DRV8871 datasheet 7.3.3) | Have (2 spares). Task 9 |

## Wiring and assembly

| Qty | Part | Use |
| --- | --- | --- |
| 1 | Protoboard, about 50 x 70 mm | Limit input filter board (10 channels) |
| 6 | Screw terminal, 2-pole, 5.08 mm pitch | 10 switch wires + GND in on the filter board (12 poles) |
| 1 | Male header strip or 12-way Dupont cable | Filter board out to the Mega pins D3-D12, 5 V, GND |
| 1 | Star-ground terminal block or bus bar | Single ground point |
| - | Hook-up wire, 22 AWG stranded | Signals, panel rewire to D23/D24 |
| - | Power wire, gauge TBD (open item 1) | 12 V supply, fuse, relay, motor feeds |
| - | Heat-shrink, cable ties | Insulating cap leads, twisting motor leads |

## Future (stretch S1, not to buy yet)

| Ref | Qty | Part | Use |
| --- | --- | --- | --- |
| U4 | 1 | Bidirectional level shifter 5 V / 3.3 V (BSS138 type) | Mega Serial1 to ESP32 |
| U5 | 1 | ESP32 board | Bridge and mesh node |

## Order list (what to buy now)

Supply caps, motor caps and the relay are fitted (todo H1/H3). Still needed for the limit filters:
- 10 x 100 nF 50 V X7R ceramic, leaded (from the 50-pack)
- 21 x 1 kOhm 1/4 W metal film (20 filters + R1; buy a 100-pack)
- Spare 3 A time-lag fuses (match the existing holder)
- Protoboard, 2-pole 5.08 mm screw terminals (6), header pins or Dupont cable, 22 AWG wire, heat-shrink
