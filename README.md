# Joust retrofit

Hardware notes and test firmware for the Joust motor retrofit.

| Folder | Contents |
| --- | --- |
| [`motor_bench_test/`](motor_bench_test/) | Endurance test firmware and overnight logger (PlatformIO, Arduino Mega 2560) |
| [`ref/`](ref/parts.md) | Parts list, datasheets, and which specs are confirmed |

## Motor bench test

Two N20 encoder gearmotors and two optional 8 mm steppers cycle between positions A and B until stopped. Flags on the shafts are compared against marks on a backdrop to measure position drift.

| | |
| --- | --- |
| Cycle | A → B (1000 ms), pause 500 ms, B → A (1000 ms), pause 500 ms |
| Rate | 3 s per cycle: 1,200 per hour, 12,000 in 10 hours |
| N20 travel | 45° closed loop on the encoder; brake during pauses |
| Stepper travel | 3 full steps, 54° if the motor is 18°/step; coils off during pauses |

**Why 54° and not 45°:** 45° needs 5 half-steps, which leaves one end between detents. The rotor would jump when the coils release, so the flag marks wouldn't mean anything.

### Power

```
Separate 5 V supply (1 A+) ──► VCC on all 4 DRV8833 modules
Laptop USB ──────────────────► Mega (power and data)
Mega 5V pin ─────────────────► N20 encoder VCC only
GND shared by everything
```

- **Don't power the motors from the Mega's 5V pin.** On USB that rail has about 500 mA to share with the Mega itself, and N20 stall current is unknown. An overload could reset the board, which is exactly what this test is trying to catch.
- **Don't connect the 5 V supply to the Mega's 5V pin.** Only its GND joins the Mega. A spare USB charger works as the supply.
- **Add a 100 µF capacitor** across the motor 5 V rail, near the modules.

| 5 V motor rail load | Current |
| --- | --- |
| Steppers: 2 × one coil × (5 V / 39.2 Ω) | 255 mA |
| N20: 2 × 150 mA listed (at 6 V; stall current unknown) | 300 mA |
| **Total** | **~555 mA** |

### Stopping USB resets

A Mega resets whenever the serial port opens. If the logger reconnects overnight, that would look like a real reset and cost us the position reference.

**Fit a 10 µF capacitor between RESET and GND on the Mega (+ to RESET) before starting the test.** Remove it for uploads, or the upload will fail. The value comes from community reports rather than Arduino, so confirm it: restart the logger and check that no new `BOOT` line appears.

### Wiring

Each motor gets its own DRV8833 module. Module pin names are read from a listing photo, so check them against the board.

| Motor | Connection | Mega pin |
| --- | --- | --- |
| N20 1 | Module IN1 / IN2 | D9 / D10 |
| N20 1 encoder | C1 / C2 | D2 / D3 |
| N20 2 | Module IN1 / IN2 | D5 / D6 |
| N20 2 encoder | C1 / C2 | D18 / D19 |
| Stepper 1 | Module IN1 / IN2 / IN3 / IN4 | D22 / D23 / D24 / D25 |
| Stepper 2 | Module IN1 / IN2 / IN3 / IN4 | D26 / D27 / D28 / D29 |
| N20 modules, optional | ULT (nFAULT) | D30 |
| Stepper modules, optional | ULT (nFAULT) | D31 |

N20 harness, in connector order. C1 and C2 go directly to the Mega, not through the driver module:

| Wire | Red | Black | Yellow | Green | Blue | White |
| --- | --- | --- | --- | --- | --- | --- |
| Label | M2 | VCC | C2 | C1 | GND | M1 |
| Goes to | Module OUT2 | Mega 5V | Mega D3 (motor 2: D19) | Mega D2 (motor 2: D18) | Mega GND | Module OUT1 |

> **Black is encoder power and blue is ground, so red and black are not power.** Red is a motor wire. Check with a meter before powering: M1–M2 should read as a motor winding, and VCC–GND should not be a short. The listing doesn't give the encoder's voltage. 5 V is based on a similar N20 that lists the same pinout and 3.3 V or 5 V.

- **Stepper coils:** coil A on OUT1 / OUT2, coil B on OUT3 / OUT4. Confirm each pair reads about 39 Ω.
- **EEP pin (nSLEEP):** the chip sleeps unless this is high. If the module's `J1` link isn't closed, tie EEP to 5 V.

### Setup

Commands below are sent from `motor_bench_test/`.

```bash
pio run -t upload        # Capacitor off. A new build ignores saved cycle totals.
systemd-inhibit --what=sleep:idle:handle-lid-switch python3 tools/bench_logger.py
```

1. **Stepper angle:** `echo "t 1 20" > logs/cmd` should turn the flag exactly once. Repeat with `t 2 20`.
2. **N20 encoder:** `echo "c 1 60" > logs/cmd`, let it make whole turns, then `echo x > logs/cmd`. Set `N20_COUNTS_PER_REV` in `include/config.h` to counts ÷ turns, and re-flash. Repeat for motor 2. If it reports `ENCODER_REVERSED`, set `N20_x_ENC_INVERT`.
3. **Check the stops:** send `b`, then `a`, and watch the steppers as they stop. If a flag moves when its coils release, set `STEPPER_TWO_COIL = true`.
4. **Fit the RESET capacitor**, then restart the logger and confirm no new `BOOT` line appears.
5. **Mark A:** send `z`, then mark all four flags.
6. **Mark B:** send `b`, mark all four flags, then send `a`.
7. **Start:** send `g`.
8. **Heat check:** touch the steppers after 15 minutes. If they're too hot to hold, raise `DWELL_MS`.

### Commands

| Command | Action |
| --- | --- |
| `g` | Start the test |
| `x` | Finish the current cycle, stop at A, print totals |
| `s` | Status |
| `a` / `b` | Move to A / B |
| `z` | Treat the current position as A |
| `c <1\|2> <pwm>` | Run an N20 open loop; `x` stops it and prints counts |
| `t <1\|2> <steps>` | Step a stepper |
| `n` | Clear saved totals |

### Reading the results

```bash
python3 tools/bench_logger.py --summary logs/run-XXXX.log
```

The log gives the total cycles and the time of each reset. The Mega's bootloader hides the chip's reset flags, so the cause is best-effort:

| `BOOT reset=` | Meaning |
| --- | --- |
| `WATCHDOG` | The firmware hung |
| `WARM` | Reset button, USB, or a brief power dip |
| `POWER_ON` | Power was lost |

- **Trust the logger's total over the board's.** The board saves to EEPROM every 100 cycles, so it can lose up to 99 per reset. Its copy is the backup if the laptop drops out.
- **A reset ends the drift measurement.** The board keeps cycling for wear data, but the encoders are incremental: they restart at zero with no memory of position, and the stepper phase is lost too. Drift can only be compared up to the first reset; the log records which cycle that was.
- **`e1=` and `e2=` are encoder errors in counts**, listed as A/B. They show if an N20 is missing its target, but not gearbox slip or backlash. Only the flags show those, and only the flags can show stepper drift, since the steppers have no feedback.

### Status

Firmware compiles for the Mega and the logger passed against a simulated board. Neither has run on real hardware, so the N20 loop gains in `config.h` are untuned and the `BOOT reset=` labels are unconfirmed.
