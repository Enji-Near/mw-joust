# Implementation Plan: Joust firmware v2 (lance fix + overhaul)

Status: APPROVED by owner 2026-11-07 (Tasks 1-4). Tasks 1-4 built, Checkpoint A waiting on owner review. Branch `firmware-v2`.
Date: 2026-11-07

## Overview

Rewrite the Joust firmware so the lance up/down (LUD) motors stop reliably at their posts and never drive into them for more than a bounded time, then extend the same motion code to rotation and the chain drive. Keep the existing lance motors and the Adafruit Motor Shield V2 (owner decision: fix with caps and code, no servo/driver swap). Move the chain drive from relays to a DRV8871. Add a bench/calibration firmware like the Space Ships project, and leave clean hooks for the ESP32 bridge and choreographed "life" moves later. ezButton is removed entirely (owner request); all inputs use the project's own `SwitchFilter`.

Old firmware: `2025_04_02_Joust_Deployment_NRG/2025_04_02_Joust_Deployment_NRG.ino` (1683 lines, Arduino Mega 2560, ezButton, Adafruit_MotorShield).

## Root causes of the LUD failures (from the review)

1. **On lance timeout the motor is never stopped.** `lowerLances()` / `raiseLances()` / `engage_Kx_Lance*()` set `Kx_Check = true` after 3 s but never call `Kx_lance_STOP()`. The motor keeps pushing on the post until the next lance command (for example the whole 9 s chain drive during CHARGE). Old .ino lines 1045-1052, 1140-1147, 1194-1200, 1244-1250, 1295-1301, 1351-1358.
2. **A noise glitch right after start marks the lance as "already there".** Pattern `if (open) start(); if (!open) done = true;` re-reads the raw pin 100 ms after starting the motor. One noisy read = that lance is never watched and never stopped. Lines 988-1010, 1080-1102, 1167-1175, 1217-1225, 1268-1276, 1317-1329.
3. **The switch is sampled every ~300 ms** (three `delay(100)` per loop pass) and ezButton needs two agreeing samples, so the motor stalls against the post for 300-600 ms on every good stop. Under continuous motor noise any glitch restarts the debounce.
4. Weak internal pull-up (20-50 kOhm) on long wires next to motor leads. The bolt side is grounded through a brush (owner, Q1), a second sliding contact that more wetting current also helps.

## Architecture decisions (each gets an ADR in `docs/decisions/`)

| # | Decision | Why |
| --- | --- | --- |
| ADR-001 | PlatformIO project `firmware/` with envs `joust` (live), `bench` (calibration), `native` (host unit tests). Shared motion code in `lib/motion`. | Same pattern as Space Ships: what is tuned on the bench behaves the same live. Native tests prove the filter, profile and Axis without hardware. |
| ADR-002 | One `Axis` state machine for all five motors (K1/K2 lance, K1/K2 rotate, chain), non-blocking, ~1 ms loop. Driver backends behind `MotorDriver`: Adafruit shield port, relay pair, DRV8871. | Removes the 14 near-duplicate engage/raise/lower functions; lets moves overlap (needed for "life" choreography). |
| ADR-003 | Integrating switch filter, time-based (ms of accumulated active time), each sample capped at 4 ms, seeded from raw reads at boot. Replaces ezButton for every input. | Tolerates a fraction of noisy samples; a steady-time debounce never settles under continuous noise. The cap stops one read after a stalled loop from flipping the state. |
| ADR-004 | Open-loop time-based decel: cruise, S-curve down to CRAWL starting from the *fastest* calibrated trip, crawl to the switch. Cutoff per axis and direction = slowest trip on that profile + small margin. Every exit path stops the motor. | No encoders. The cutoff bounds damage when a switch is missed. |
| ADR-005 | Calibration: `config.h` holds defaults (pasted from the bench block); the bench can also save a calibration to EEPROM, which the live firmware uses when present and valid (version + CRC). Boot prints which source is in use; a bench command clears the EEPROM copy. | Owner chose "both" (Q3): values visible in git, and re-calibration on site without a laptop build. Values are only written by an explicit bench command, never learned at runtime. |
| ADR-006 | Line-based text command protocol, identical over USB and (later) Serial1 to an ESP32. Mega keeps all motion and safety; ESP32 is only a bridge. | Remote access later without touching motion code; safety stays on the board with the switches. |
| ADR-007 | Chain drive on DRV8871, D44/D45 (Timer5, PWM raised above hearing). Timer0/millis untouched. Chain runs at 12 V, within DRV8871 VM 6.5-45 V. | Relays can't do speed or decel. Timer5 pins verified: `variants/mega/pins_arduino.h` lines 359-361 (D44 = OC5C, D45 = OC5B, D46 = OC5A). |
| ADR-008 | Move guards: the origin switch must open within T ms; a limit behind the motor closing = STUCK_OR_REVERSED; both limits closed = WIRING_FAULT; a target closure before a minimum travel time is ignored. Any fault puts that axis in FAULTED. | Catches a stuck motor, a swapped motor lead, or a broken switch wire on the first move instead of stalling into a post every cycle. |
| ADR-009 | Shield I/O hardening: Wire timeout on and checked after every shield write; idle axes re-sent STOP every 100 ms; I2C written only when duty changes; Wire at 400 kHz; `AFMS.begin()` first in setup, never blocking before the watchdog. | A dropped STOP leaves a motor running. `begin()` zeroes all 16 channels (`Adafruit_MotorShield.cpp:77-78`), but the PCA9685 keeps running motors through a Mega reset until then. |
| ADR-010 | Fault policy (owner, revised again 2026-11-07). **Any fault locks the show at once**: every motor stops, motor power off, fault saved to EEPROM. No recovery moves, no retries (a retry can push a lance into its post again, and with a broken ground neither post can be seen). Start button blinks the fault class: **lance fault 500 ms on / 500 ms off; any other fault 2000 / 2000.** Full detail (knight, axis, switch, cause) on serial, for remote access. A power cycle comes back locked; unlock needs the key turned lock -> unlock (not a key left in unlock); safe-op does not bypass it. False readings are handled before this point by the switch filter (noise) and the min-travel guard, so a fault means a real miss. | The lance post is the hardest part to repair; staff must look before it moves again, and the public won't report a show running without lances. |
| ADR-012 | Indicator: the start-button LED only (owner). Patterns: lance fault 500/500 ms, other fault 2000/2000 ms, locked waiting for key 3000/3000 ms, manual mode 1000/1000 ms (last two from v1). Remote detail via serial (ESP32 mesh or a Raspberry Pi on a hotspot, later). | No extra hardware; a blinking start button also prompts visitors to tell staff. |
| ADR-011 | Motor-power cut-off (owner, Q6): relay on PIN_MOTOR_POWER (D22, tentative; 2-relay board as placeholder, then a single-relay board) in the single 12 V supply's + line after the fuse, feeding the shield motor terminal and the chain driver. Energised only after setup has stopped every motor; dropped on any fault. Hardware must be OFF while the pin floats (reset, bootloader, upload). | A Mega reset does not reset the shield; this cuts motor power until the firmware is back in control. |

Control panel: **kept** (owner, revised 2026-11-07) until v2 is proven reliable; staff use it often and the gearing prevents moving the knights by hand. The both-lances buttons move off A4/A5 (removed by owner) to D23/D24 (tentative). Panel moves go through `Axis`, so they get the same filters, cutoffs and guards as the show.

Power (owner, 2026-11-07): one 12 V supply for the shield and the chain drive, one fuse, one motor-power relay. The two supplies were split in v1 to keep chain noise off the shield; Task 5 watch data (switch glitch counts with the chain running) checks the merge. Legacy fuse: 3 A, assumed time-lag. The Mega stays on USB. All grounds meet at one point (the DRV8871 inputs are referenced to Mega GND).

Deferred: timed moves (move for N ms instead of to a limit) for choreography, added in S3 alongside the motion-limit survey.

Verified facts this plan relies on:
- Adafruit_MotorShield 1.1.4 `run()` handles FORWARD, BACKWARD, RELEASE only; BRAKE is defined but not implemented (`Adafruit_MotorShield.cpp:231-244`). RELEASE = coast. `setSpeed(s)` writes `s * 16` (line 255), so old lance 50 = 800/4095 and rotate 150 = 2400/4095 in `setSpeedFine()` units.
- AVR Wire (framework-arduino-avr 5.4.0) has `setWireTimeout()` but it is **off by default** (`twi.c:54`, `twi_timeout_us = 0`).

## Dependency graph

```
config.h (pins, polarity, tunables, calibration defaults)
   │
   ├── SwitchFilter (pure) ───────────────┐
   ├── SpeedProfile (pure) ───────────────┤
   │                                       ▼
   │                                 Axis state machine (pure, MotorDriver interface)
   │                                       │
   ├── Drivers: ShieldDriver, RelayDriver → Drv8871Driver
   │                                       │
   │              ┌────────────────────────┴───────────────┐
   │              ▼                                        ▼
   │        bench env (jog, watch, calibrate)      joust env (lock/key, lockout, home, show)
   │              │                                        │
   │              └──── calibration ─► config.h / EEPROM ◄─┘
   │
   └── Console (command protocol) ── later: ESP32 bridge
```

## Task list

See `tasks/todo.md` for acceptance criteria and verification per task.

### Phase 0: Hardware (owner, in parallel)
- [ ] H1: Noise suppression, caps placed as in todo
- [ ] H2: Repair signal lines, re-solder broken post
- [ ] H3: Fuses and supply caps; current measurements; motor-power relay

### Phase 1: Foundation (no hardware needed)
- [x] Task 1: PlatformIO project, config.h, native test env, project CLAUDE.md
- [x] Task 2: SwitchFilter + tests
- [x] Task 3: SpeedProfile + tests
- [x] Task 4: Axis state machine + tests (fake driver)

### Checkpoint A: tests pass on host, `pio run` clean for Mega, owner review of the Axis interface

### Phase 2: Bench firmware (first hardware contact, owner runs it)
- [ ] Task 5: Drivers + bench jog + switch watch with glitch counter
- [ ] Task 6: Calibration, print config block, save/clear EEPROM copy

### Checkpoint B: noise measured before/after caps; calibration values in config.h

### Phase 3: Live firmware
- [ ] Task 7: Port the show (lock/key, safe-op, home, joust, winner) onto Axis, constant speed; persistent lockout; motor-power relay
- [ ] Task 8: Turn on decel for lances, then rotation
- [ ] Task 9: Chain drive on DRV8871 + decel
- [ ] Task 10: Port the control panel onto Axis moves (both-lances buttons on D23/D24)

### Checkpoint C: soak test, target 0 cutoffs in 300 runs (old: ~4 lance cutoffs in 150)

### Phase 4: Documentation
- [ ] Task 11: README, wiring diagram (handoff: `docs/handoff-wiring-diagram.md`), ADRs, bench guide (owner); operations manual update (client, with owner contact details)

### Phase 5: Stretch (each gets its own short plan before code)
- [ ] S1: ESP32 bridge on Serial1 + mesh node (`security-and-hardening` first)
- [ ] S2: Limit-of-motion survey (manual jog, owner reports safe partial rotations near the divider)
- [ ] S3: Choreography: timed moves, collision, winner flourish, loser bow (`observability-and-instrumentation` for remote monitoring)

## Risks and mitigations

| Risk | Impact | Mitigation |
| --- | --- | --- |
| Lance faults keep happening after the upgrades: each one locks the show | Med | Checkpoint C soak (0 cutoffs in 300 runs) passes before the client runs v2; glitch counts show whether the filter or the hardware needs work. |
| Merged supply brings chain noise back onto the shield | Med | Compare glitch counts in Task 5 with the chain on the shared supply; revert to two supplies if they rise. |
| Travel time varies too much (temperature, supply sag, wear) for time-based decel | High | Measure spread in Task 6; decel from the fastest trip; crawl is slow enough that late decel is harmless. |
| Switch still unreadable even with caps + filter | High | Glitch counter in Task 5 quantifies it; hard cutoff limits damage; further hardware fix decided from data. |
| I2C (motor shield) disturbed by motor noise | Med | ADR-009. |
| Rewrite regresses rotation/chain behaviour that works today | Med | Task 7 ports behaviour at constant speed first and is soak-tested before decel is added. |
| Motors keep running through a Mega reset, or if the I2C bus is dead | Med | ADR-009 + ADR-011 motor-power relay. |
| Post is both the sensor and the hard stop: detection happens at impact | Med | Decel makes the impact a crawl. |
| DRV8871 coasts where the relays may have braked | Med | Stop chain with brake (both inputs high); confirm relay wiring first. Timer5 reserved (no Servo library). |
| EEPROM contents surviving a firmware upload | Low | Unverified for this upload path; check on the bench before Task 7 relies on it. |

## Design review log

- Doubt-driven review, cycle 1 (fresh-context adversarial reviewer, 2026-11-07): 11 findings, all valid; folded in as ADR-003/004 changes, ADR-008, ADR-009, risks above, and Task 4/5/7 criteria.
- Cross-model second opinion: declined by owner. Proceeding with single-model findings only.
- 2026-11-07, owner answers: Q1 brush ground; Q2 all motors 12 V, chain at full 12 V; Q3 calibration in both config.h and EEPROM; Q4 retire the panel; Q5 any fault locks, lockout survives power cycles; Q6 add motor-power relay; Q7 owner work contacts are public and belong in the client manual. Axis simplified: any fault -> FAULTED (replaces "3 consecutive cutoffs -> DISABLED").
- 2026-11-07, owner answers round 2: shield fed from 12 V; 3 A legacy fuse; DRV8871 confirmed (2 spares); panel kept (Q4 reversed); lance faults recover instead of locking; indicator wanted; 2-relay board as motor-power placeholder; A4/A5 buttons removed.
- 2026-11-07, round 3: single supply + single relay; any lance fault locks (no recovery, no retries); start button is the only indicator; Mega stays on USB; owner delegated the Axis interface review to the tests.
- Skills adopted for this project: test-driven-development, incremental-implementation, source-driven-development; later security-and-hardening (S1), observability-and-instrumentation (S3).

## Open questions
1. Rest-lost check (proposed, not built): an idle lance whose post switch opens with no move commanded = ground/brush or that post's wire. Lock without moving. Needs: does the bolt stay on the up post when the motor is released?
2. Homing a lance from an unknown position (neither switch closed): allow only after a key unlock (operator present), at crawl with a tight cutoff; never in unattended safe-op boot. OK?
