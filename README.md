# Joust retrofit

Hardware notes and test firmware for the Joust motor retrofit.

| Folder | Contents |
| --- | --- |
| [`motor_bench_test/`](motor_bench_test/) | Endurance test firmware and overnight logger (PlatformIO, Arduino Nano Every) |
| [`ref/`](ref/parts.md) | Parts list, datasheets, and which specs are confirmed |

## Motor bench test

Two N20 encoder gearmotors and two 8 mm steppers cycle between positions A and B until stopped. Flags on the shafts are compared against marks on a backdrop to measure position drift.

| | |
| --- | --- |
| Cycle | A → B (1000 ms), pause 500 ms, B → A (1000 ms), pause 500 ms |
| Rate | 3 s per cycle: 1,200 per hour, 12,000 in 10 hours |
| N20 travel | 45° closed loop on the encoder; brake during pauses |
| Stepper travel | 3 full steps, 54° if the motor is 18°/step; coils off during pauses |

**Why 54° and not 45°:** 45° needs 5 half-steps, which leaves one end between detents. The rotor would jump when the coils release, so the flag marks wouldn't mean anything.

### Power

```
12 V supply ──► Nano VIN          GND shared by everything
Nano +5V ─────► VCC on all 4 DRV8833 modules
Laptop USB ───► Nano (data)
```

> **Never put 12 V on a DRV8833 module.** Its absolute maximum is 11.8 V and its operating maximum is 10.8 V.

The 12 V must stay connected, because USB alone can't carry the motors. Add a 100 µF capacitor across the 5 V rail near the modules so motor noise doesn't reset the board.

| 5 V rail load | Current |
| --- | --- |
| Steppers: 2 × one coil × (5 V / 39.2 Ω) | 255 mA |
| N20: 2 × 150 mA listed (at 6 V; stall current unknown) | 300 mA |
| **Total, against 950 mA available** | **~555 mA** |

### Wiring

Each motor gets its own DRV8833 module. Module pin names are read from a listing photo, so check them against the board.

| Motor | Module pin | Nano pin |
| --- | --- | --- |
| N20 1 | IN1 / IN2 | D9 / D10 |
| N20 1 encoder | A / B | D2 / D4 |
| N20 2 | IN1 / IN2 | D3 / D5 |
| N20 2 encoder | A / B | D7 / D8 |
| Stepper 1 | IN1 / IN2 / IN3 / IN4 | A0 / A1 / A2 / A3 |
| Stepper 2 | IN1 / IN2 / IN3 / IN4 | A4 / A5 / A6 / A7 |
| N20 modules, optional | ULT (nFAULT) | D6 |
| Stepper modules, optional | ULT (nFAULT) | D11 |

- **N20 motor wires:** OUT1 / OUT2.
- **N20 encoder power:** assumed 5 V from the Nano. The listing doesn't give the encoder pinout or voltage, so confirm before powering it.
- **Stepper coils:** coil A on OUT1 / OUT2, coil B on OUT3 / OUT4. Confirm each pair reads about 39 Ω.
- **EEP pin (nSLEEP):** the chip sleeps unless this is high. If the module's `J1` link isn't closed, tie EEP to 5 V.

### Setup

Commands below are sent from `motor_bench_test/`.

```bash
pio run -t upload        # Flashing clears the saved cycle totals
systemd-inhibit --what=sleep:idle:handle-lid-switch python3 tools/bench_logger.py
```

1. **Stepper angle:** `echo "t 1 20" > logs/cmd` should turn the flag exactly once. Repeat with `t 2 20`.
2. **N20 encoder:** `echo "c 1 60" > logs/cmd`, let it make whole turns, then `echo x > logs/cmd`. Set `N20_COUNTS_PER_REV` in `include/config.h` to counts ÷ turns, and re-flash. Repeat for motor 2. If it reports `ENCODER_REVERSED`, set `N20_x_ENC_INVERT`.
3. **Check the stops:** send `b`, then `a`, and watch the steppers as they stop. If a flag moves when its coils release, set `STEPPER_TWO_COIL = true`.
4. **Mark A:** send `z`, then mark all four flags.
5. **Mark B:** send `b`, mark all four flags, then send `a`.
6. **Start:** send `g`.
7. **Heat check:** touch the steppers after 15 minutes. If they're too hot to hold, raise `DWELL_MS`.

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

The log gives the total cycles, and the time and cause of each reset.

- **Trust the logger's total over the board's.** The board saves to EEPROM every 100 cycles, so it can lose up to 99 per reset. Its copy is the backup if the laptop drops out.
- **A reset ends the drift measurement.** The board keeps cycling for wear data, but the encoders restart at zero and the stepper phase is lost. Drift can only be compared up to the first reset; the log records which cycle that was.
- **`e1=` and `e2=` are encoder errors in counts**, listed as A/B. They show if an N20 is missing its target, but not gearbox slip or backlash. Only the flags show those, and only the flags can show stepper drift, since the steppers have no feedback.

### Status

Firmware compiles and the logger passed against a simulated board. Neither has run on real hardware, so the N20 loop gains in `config.h` are untuned starting values.
