# Joust v2 bring-up checklist (before calibration)

Work top to bottom; don't skip a stage that fails. Write readings in the `Reading:` blanks.
Wiring reference: `docs/wiring/` (schematic PDF, pin table). Net names below match the schematic.

> [!warning] Rules
> - Nothing that moves a motor runs without a deliberate decision (yours) at that step.
> - The Mega probably still runs **v1**. v1 doesn't use D22, so the motor-power relay stays OFF and motors can't move. **Don't energise the relay until the v2 skeleton is flashed (Stage 3).**
> - Meter on DC volts unless stated. Black probe on the star ground.

> [!info] Where expected values come from
> - Arithmetic from the drawings (1 kOhm pull-up, 1 kOhm series, 100 nF).
> - ATmega2560 input thresholds: LOW below 0.3 x Vcc (1.5 V), HIGH above 0.6 x Vcc (3.0 V). From the datasheet's DC characteristics; table not re-checked here.
> - "Record" = no known value yet; your reading becomes the reference.

---

## Stage 0: Power OFF (USB unplugged, 12 V off)

- [ ] Wait 1 minute, then measure across C1 (330 uF). **Expect < 0.5 V** before touching anything. Reading: ____
- [ ] Electrolytic polarity: stripe (−) to GND on C1 and C3 (and C9 if the DRV8871 is fitted).
- [ ] Shield **Vin jumper removed**.
- [ ] Fuse F1 in its holder: **3 A time-lag** (T3A).
- [ ] TIP120 tab not touching metal (the tab is the collector), or insulated.
- [ ] Ohms, star ground to each GND below. **Expect < 1 Ohm** each.
  - [ ] PSU − : ____
  - [ ] Mega GND pin: ____
  - [ ] Shield GND / VMOT − terminal: ____
  - [ ] Motor-power relay board GND: ____
  - [ ] Chain relay board GND: ____
  - [ ] Filter board GND: ____
  - [ ] TIP120 emitter (right leg): ____
- [ ] Ohms, `+12V_FUSED` to GND. **Expect the reading to climb** (C1 charging from the meter) to high kOhm or OL. **Fail: stays under about 10 Ohm** (short). Reading: ____
- [ ] Ohms, `+12V_SW` (relay NO side / shield motor terminal) to GND. Same: **climbs; fail if it stays under about 10 Ohm.** Reading: ____
- [ ] Ohms, `+12V_FUSED` to `+12V_SW` (relay off). **Expect OL** (open contact). Fail: near 0 Ohm = relay wired COM to NC, or welded. Reading: ____
- [ ] Open item 3: continuity, shield A4 to SDA, and A5 to SCL. **Record** (beep / no beep): A4-SDA ____ A5-SCL ____

## Stage 1: USB only (12 V still off)

Plug the Mega into USB. Whatever firmware is on it runs, but motor power is off.

- [ ] Mega 5V pin. **Expect 4.6-5.25 V** (USB; the board's input protection drops a little). Reading: ____
- [ ] Mega VIN pin. **Expect well under 12 V** (12 V is off anyway; this is the baseline for Stage 2). Reading: ____
- [ ] Motor-power relay: **OFF** (no click, relay LED off). Press the Mega reset button: still OFF.
- [ ] Start LED: off (it runs from 12 V, which is off).
- [ ] Each limit input, measured **at the Mega pin**. Expect:
  - Open: **about the 5V-pin reading** (4.6-5.25 V).
  - Closed (bolt on post): **under 0.25 V** = good contact (under about 50 Ohm through post, bolt and brush).
  - Closed, 0.25-1.5 V: **marginal**: clean the post/brush. Above 1.5 V the Mega may read it as open.
  - If you can turn an axle by hand, check both states; otherwise record the state it's in now and check the other in Stage 4.

| Pin | Switch | State now | Volts | Other state | Volts |
| --- | --- | --- | --- | --- | --- |
| D3 | K1 lance down [B'] | | | | |
| D4 | K2 lance down [B] | | | | |
| D5 | Chain CCW [A] | | | | |
| D6 | Chain CW [A'] | | | | |
| D7 | K1 lance up [C'] | | | | |
| D8 | K2 lance up [C] | | | | |
| D9 | K1 rotate CCW [D'] | | | | |
| D10 | K2 rotate CCW [D] | | | | |
| D11 | K1 rotate CW [E'] | | | | |
| D12 | K2 rotate CW [E] | | | | |

- [ ] No lance shows **both** its up and down switch closed (D3+D7, D4+D8). Same for rotate (D9+D11, D10+D12) and chain (D5+D6). Both closed = a wiring fault.

## Stage 2: 12 V on, relay still OFF

Turn the 12 V supply on. Motors can't move: `+12V_SW` is dead while the relay is off.

- [ ] PSU output, no load. **Expect 11.4-12.6 V** (12 V ±5 %). Reading: ____
- [ ] `+12V_FUSED` after the fuse. **Expect the same as the PSU, within 0.05 V.** Reading: ____
- [ ] `+12V_SW` (shield motor terminal). **Expect under 0.5 V** (relay off). **Fail: about 12 V → turn off at once; the relay is wired wrong or stuck.** Reading: ____
- [ ] Mega VIN pin. **Expect the same as Stage 1** (no 12 V). **Fail: about 12 V = shield Vin jumper still on → turn off, remove it.** Reading: ____
- [ ] Volts, PSU − to Mega GND. **Expect under 0.05 V.** Reading: ____
- [ ] Start LED current: meter (mA range) in series, `+12V_FUSED` → LED + → LED − → meter → GND (TIP120 bypassed). **Expect 5-25 mA, LED cool and steady.** Over about 30 mA, or the LED gets warm: it needs a series resistor (680 Ohm, ½ W) before going further. Reading: ____ mA
- [ ] Start LED through the TIP120, driven by the firmware on D13 (v1 blinks it while waiting for the key). **Expect the LED to blink.** While lit, **TIP120 collector to emitter under about 1.2 V** (unverified; Darlington saturation). Don't jumper D13 to 5 V: the firmware drives it as an output. Reading: ____ V
- [ ] Idle current from the PSU (meter in series, 10 A range). **Record.** Only the start LED should draw (if lit). **Fail: over about 0.3 A with nothing running.** Reading: ____

## Stage 3: Flash the v2 skeleton, then energise motor power (owner's OK)

The `joust` skeleton stops every shield motor at boot, holds the chain relays OFF, then idles. It never drives D22, so the relay stays off.

- [ ] **Decision: flash the `joust` skeleton** (`pio run -e joust -t upload` from `firmware/`). Serial at 115200 shows `Joust v2 joust skeleton: motors stopped.` and **no** `FAULT: motor shield not found on I2C`.
- [ ] Relay still OFF during and after the upload.
- [ ] **Decision: energise the motor-power relay by hand.** Most boards: jumper the relay IN to GND (active LOW). Motors must stay still: the skeleton has stopped them.
- [ ] `+12V_SW` at the shield motor terminal. **Expect within 0.1 V of `+12V_FUSED`.** Reading: ____
- [ ] `+12V_SW` at the chain relay board V+. **Same.** Reading: ____
- [ ] Idle current with motor power on. **Record.** **Fail: over about 0.3 A with no motor moving.** Reading: ____
- [ ] No motor moves, hums or warms. Touch-test each motor after 1 minute.
- [ ] Remove the IN jumper: relay drops, `+12V_SW` falls toward 0 V (C3 and C9 bleed down over a few seconds). Reading after 10 s: ____

## Stage 4: Bench firmware checks (after Task 5 is built)

Needs the Task 5 bench firmware (not written yet). Each step that moves a motor is a separate decision.

- [ ] **Decision: flash `bench`.** It reports the shield on I2C at 0x60.
- [ ] Switch watch, nothing moving: all 10 limits match Stage 1; **glitches per second = 0** on every input.
- [ ] Switch watch, operator inputs (internal pull-ups): start D2, key A9 (lock → unlock), safe-op A12 jumper, panel A2, A3, A10, A11, D23, D24. Each reads pressed/released correctly.
- [ ] Start LED: blink command works (D13).
- [ ] **Decision: jog each axis, one at a time, slowest speed.** For each, record: direction correct? stops on its limit? running current (meter in the 12 V line).

| Axis | Direction matches config.h? | Stops on limit? | Running current (A) | Glitches/s on its limits while running |
| --- | --- | --- | --- | --- |
| K1 lance (down = FORWARD) | | | | |
| K2 lance (down = FORWARD) | | | | |
| K1 rotate (CW = BACKWARD) | | | | |
| K2 rotate (CW = BACKWARD) | | | | |
| Chain (A0 = CCW, A1 = CW) | | | | |

- [ ] Chain running current. **Expect under 2.1 A** (DRV8871 board current limit; above it the chain slows). Reading: ____
- [ ] Glitch counts on all lance limits with the chain running. **Expect 0**; record anything else (Checkpoint B data).
- [ ] Lowest voltage at the shield motor terminal while the chain starts. **Record.** Reading: ____
- [ ] Relay cut-off under load: drop the relay while a motor jogs. **Expect the motor to stop at once.**
- [ ] Combined current over a full v1-style cycle (H3). **Expect the highest steady reading under about 2 A** to keep the 3 A fuse. Reading: ____

## Done → Task 6 calibration

- [ ] All stages pass; readings saved in this note.
- [ ] Wrong directions or pins found above are fixed in `firmware/include/config.h`, not in the wiring notes.
