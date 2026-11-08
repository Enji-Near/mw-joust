# Joust firmware v2: task list

Plan: `tasks/plan.md`. Build: `pio run -e joust -e bench` from `firmware/`. Host tests: `pio test -e native`.
Rule: nothing that moves hardware is uploaded or run without the owner's OK.

## Phase 0: Hardware (owner)

### H1: Motor noise suppression
- [ ] 0.1 uF ceramic directly across each motor's two terminals (or as close as possible). Optional: 0.1 uF from each terminal to the motor case.
- [x] **Do not put the 100 uF electrolytic across a motor.** The lance motors reverse, so a polarised cap gets reverse-biased every other move. A big cap on a PWM'd driver output also pulls a current spike on every PWM edge. 100 uF goes across the motor supply instead (see H3). ✅ 2026-11-07
- [ ] Twist each motor's two leads together; route switch wires away from motor wires where possible.
- [ ] Recommended: at the Mega, each lance switch input gets a 1 kOhm pull-up to 5 V (about 5 mA through the bolt/post and brush contacts helps break through oxide and grease; the internal pull-up gives ~0.1-0.25 mA), then 1 kOhm series and 100 nF to GND at the pin.
- [x] Dedicated ground path to each bolt (brush), not through the gearbox shaft and bearings. ✅ 2026-11-07
- [ ] Check each ground brush: clean contact, spring pressure, wire intact (it is a second sliding contact in the switch path).

### H2: Repairs
- [ ] Repair damaged signal lines
- [x] re-solder the broken post. ✅ 2026-11-07
- [ ] Meter check each lance switch from the Mega pin to GND: open with lance mid-travel, near 0 ohm when the bolt rests on the post (rotate the axle slightly: the reading should stay low through the brush).
- [x] Remove the panel's A4/A5 buttons (they may share the shield's I2C lines). ✅ 2026-11-07
- [ ] Rewire Both-Lances-Up / Down to D23 / D24 (tentative, see wiring diagram).

### H3: Power inputs
- [x] Measure each supply's current over a full show (meter in series on the 10 A range, or an inline DC power meter). Record the highest steady reading. The chain motor's running current also sizes the DRV8871. ✅ 2026-11-07
- [ ] Fuse each 12 V supply at its + output, first thing after the supply: 3 A time-lag (legacy value), lower if a supply or its wire is rated below 3 A.
- [ ] At each supply input (before the motor-power relay): 330 uF + 0.1 uF ceramic. At the shield motor terminal and the DRV8871 VM (after the relay): 100 uF + 0.1 uF. Electrolytics 25 V or higher, low-ESR, polarity observed, leads short.
- [ ] Motor-power cut-off relay (ADR-011): 2-relay board as placeholder, then a single-relay board. Contacts rated above 3 A at 12 V DC. Bench check: module powered, input pin left floating -> relay must stay OFF.

### H4: Fault indicator (ADR-012)
- [ ] Choose NeoPixel strip (11+ pixels) or discrete LEDs; choose where staff will see it.
- [ ] NeoPixel wiring: 5 V + GND from a supply that can carry it (brightness capped in firmware), about 470 ohm in series on the data line, 470-1000 uF across the strip's power at the strip (Adafruit NeoPixel guide).
- [x] Q: shield motor supply voltage. ✅ 2026-11-07

## Phase 1: Foundation

### Task 1: Project skeleton ✅ 2026-11-07
- [x] `pio run -e joust` and `pio run -e bench` build for megaatmega2560 with zero warnings in project code. Both skeletons stop every motor at boot and idle, so they are safe to flash.
- [x] `pio test -e native` runs.
- [x] Every pin in the old sketch appears once in config.h with the same number (retired panel pins as a comment block).
- [x] Project `CLAUDE.md`; `lib_deps` pinned (Adafruit Motor Shield V2 1.1.4; ezButton removed).

### Task 2: SwitchFilter ✅ 2026-11-07
- [x] Clean closure detected at the first update on or after LIMIT_INTEGRATE_MS of active time, at 1 ms or 4 ms updates.
- [x] Closure with 30 % random glitches detected within 8x LIMIT_INTEGRATE_MS (50 seeded trials).
- [x] Isolated 1-3 ms spikes never report closed; each counted as a glitch. One read after a 500 ms stall can't flip the state.

### Task 3: SpeedProfile ✅ 2026-11-07
- [x] Duty never below CRAWL while moving, never above CRUISE; capped at 4095.
- [x] Decel disabled = constant duty (lets Task 7 match old behaviour).
- [x] Monotonic through each ramp; continuous at stage joins (max 8 counts per ms), including decel that starts inside the ramp-up.

### Task 4: Axis state machine ✅ 2026-11-07
- [x] Limit closes -> stop called the same tick; result `REACHED`.
- [x] No limit before cutoff -> stop called; `CUTOFF`; axis `FAULTED` until `clearFault()`. (Regression test for root cause 1.)
- [x] Glitch on the target right after start does not end the move. (Regression test for root cause 2.)
- [x] Origin still closed T ms after start, or a limit behind the motor closing -> stop, `STUCK_OR_REVERSED`. Both limits closed -> stop, `WIRING_FAULT` (moving or idle).
- [x] Mutation checks: removing the cutoff stop, the reversed-limit guard, the min-travel guard, the filter step cap, or the idle re-stop each fails a test.

### Checkpoint A
- [x] All native tests pass (32); both Mega envs build with no warnings; flash/RAM reported.
- [ ] Owner reviews the Axis interface (`firmware/lib/motion/src/axis.h`) before any hardware code.

## Phase 2: Bench firmware

### Task 5: Drivers, jog, switch watch
**Description:** `ShieldDriver` (Adafruit port; ADR-009: Wire timeout checked after every write, 400 kHz), `RelayDriver` (current chain drive). Bench serial menu at 115200: hold-to-jog each axis (dead-man like Space Ships), stop-all, watch all 10 limit inputs (raw + filtered + glitch count per second), max loop time.
**Acceptance criteria:**
- [ ] Jog stops on its limit, on key release, and never starts into a closed limit.
- [ ] Watch mode shows glitch rate per lance switch with motor running (before/after-cap numbers).
- [ ] I2C failure is reported, not hung; worst-case loop time reported and well under the watchdog period.
**Verification:** build; owner runs on hardware with permission, reports glitch counts. **Dependencies:** 4. **Scope:** M

### Task 6: Calibration
**Description:** Per axis and direction: at least 10 round trips at a chosen duty (also with the other motors running), record min/avg/max travel time (filter onset), find minimum moving duty per direction (CRAWL), print the config.h block, and optionally save it to EEPROM (version + CRC) or clear the saved copy. Owner watches each arrival.
**Acceptance criteria:**
- [ ] Prints a paste-ready block; save/clear EEPROM commands; values reload into the bench session without reflashing.
- [ ] Reports spread (max - min); decel start from the fastest trip, cutoff from the slowest + margin.
**Verification:** owner runs, pastes block, `pio run` clean. **Dependencies:** 5. **Scope:** M

### Checkpoint B
- [ ] Glitch rate measured with and without caps.
- [ ] Calibration block committed; cutoffs set to slowest trip + margin.

## Phase 3: Live firmware

### Task 7: Port the show
**Description:** Lock/key, safe-op jumper, auto-home (all original cases), joust sequence, random winner (seeded), stats. Constant speed (decel off) so it matches today's motion. Old bugs fixed (lance cutoff stops motor, `check_Safe_Operation` CW/CCW typo at old line 1372, chain confirm re-read hang at old lines 582-608, 16-bit `int` millis timers). ADR-010 persistent lockout, ADR-011 motor-power relay, calibration source (EEPROM or config.h) printed at boot.
**Acceptance criteria:**
- [ ] Every row of the operations manual behaviour table has a matching step.
- [ ] Lance fault: indicator shows the switch, lance recovers to its origin, show continues; failed recovery or 3 in a row locks. Rotation/chain fault: locks at once.
- [ ] Lock: all motors stop, motor power off, fault saved; after a power cycle still locked and showing the fault; unlock needs a key lock -> unlock change; safe-op does not bypass it.
- [ ] No `delay()` in the live loop; watchdog fed only from loop(); `AFMS.begin()` first in setup.
- [ ] Reset cause (`.noinit` marker, as in `motor_bench_test`) and fault/run counters kept in EEPROM and printed at boot.
**Verification:** build; dry run with motors unpowered (switches pressed by hand); then owner-approved run. **Dependencies:** 5, H3 relay. **Scope:** M-L (split home/show if it grows)

### Task 8: Decel
**Description:** Enable decel profile for lances, then rotation.
**Acceptance criteria:**
- [ ] Lance arrives at CRAWL on every calibrated trip (bench log shows duty at switch).
**Dependencies:** 6, 7. **Scope:** S

### Task 9: Chain drive DRV8871
**Description:** `Drv8871Driver` on D44/D45, Timer5 PWM above hearing; stop = brake (both inputs high); start at full duty to match relay timing, then calibrate and add decel. First confirm whether the relay circuit brakes or coasts today.
**Dependencies:** 8, part in hand (2 spares). Chain running current should be under about 2.1 A (Adafruit board's current limit); above that the driver chops and the chain slows. **Scope:** S-M

### Task 10: Control panel
**Description:** Port every panel button and the semi-automatic reset onto Axis moves (same filters, cutoffs and guards as the show). Both-lances buttons on D23/D24.
**Dependencies:** 7. **Scope:** S

### Checkpoint C
- [ ] 300-run soak: zero cutoffs, zero resets; log attached.

## Phase 4: Documentation

### Task 11: Docs
- [ ] `README.md`: build, upload, test, bench, calibration.
- [ ] `docs/decisions/ADR-001..011`.
- [ ] Wiring diagram (separate session, `docs/handoff-wiring-diagram.md`).
- [ ] Operations manual (client): updated behaviour, fault codes and lockout, owner contact details kept.

## Phase 5: Stretch (separate plans)
- [ ] S1 ESP32 bridge + mesh node
- [ ] S2 Limit-of-motion survey
- [ ] S3 Choreography ("life"), including timed moves
