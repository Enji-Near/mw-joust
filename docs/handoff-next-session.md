# Handoff: next Joust session (written 2026-11-08)

Start the session in the Joust repo and say: "Read `docs/handoff-next-session.md` and continue."

## Where things stand
- Branch `firmware-v2` (pushed). `main` holds only the old motor bench test; merging is the owner's call.
- Rules: read `CLAUDE.md` first, then the user-level `~/.claude/CLAUDE.md`. Commit and push at the end of every turn. Never upload or move hardware without asking. Repo is public.
- Plan: `tasks/plan.md` (ADR table, risks, open questions). Tasks: `tasks/todo.md`. The owner ticks items in Obsidian (`[x] ... ✅ YYYY-MM-DD`), so re-read before editing and keep their ticks.

## Done (2026-11-07)
- Reviewed the v1 sketch: `2025_04_02_Joust_Deployment_NRG/`. Root causes of the broken lance posts are in plan.md:
  - a lance timeout never stopped the motor;
  - one noisy read right after starting meant that lance was never watched;
  - the switches were sampled only about every 300 ms.
- Tasks 1-4 built:
  - `firmware/` PlatformIO project with three envs: `joust`, `bench`, `native`;
  - `lib/motion`: SwitchFilter, SpeedProfile and Axis, with 32 host tests and 5 mutation checks.
- Both Mega skeletons only stop every motor at boot, so they are safe to flash. Neither has been flashed.
- Checkpoint A done. The owner handed the Axis interface review to the tests.
- Wiring diagram: separate session, using `docs/handoff-wiring-diagram.md`.

## Decisions made (details in plan.md)
- **Hardware:**
  - Keep the lance motors and the Adafruit Motor Shield V2. Fix the switches with caps and code.
  - Chain drive moves to a DRV8871 (owner has 2 spares).
  - One 12 V supply, one 3 A time-lag fuse, one motor-power relay on D22 (the 2-relay board is the placeholder).
  - Mega on USB; shield VIN disconnected.
- **Control panel:** kept. The both-lances buttons move from A4/A5 (removed) to D23/D24 (tentative).
- **Faults (ADR-010):**
  - A failed move on any axis (lance, knight rotation, chain drive) locks the show at once, with no retries.
  - The lock survives a power cycle. Key lock -> unlock clears it; safe-op does not bypass it.
  - The start button is the only indicator: lance fault 500/500 ms, other fault 2000/2000 ms.
  - A lance that has merely lost contact at rest is not a fault.
- **Last known lance position (ADR-013):** stored in EEPROM. A lance is never driven toward the post it is recorded at, and at boot the record stands in for an unread switch.
- **Calibration:** `config.h` defaults, plus an optional copy saved to EEPROM by the bench firmware.

## Open questions for the owner (ask these first)
1. **EEPROM wear, the first item to settle:** does ADR-013 survive? See plan.md Open questions 1 for the numbers. The owner wants it cut if there is any real chance of wear-out over 5+ years at hundreds of lance moves a day. Ask how many shows run per day.
2. ADR-013 refinement: also record MOVING_TO_x at move start, so a mid-move power cut is recognised. OK?
3. Boot probe (owner decision, 2026-11-08):
   - when no lance switch reads closed at boot and no failure lock is saved, back off the assumed start post slightly, then return; this should close a limit;
   - open: how far to back off, and whether the probe runs in unattended safe-op boot.
4. Any measured current numbers: the merged-supply total, and the chain motor running current. The Adafruit DRV8871 board limits current at about 2.1 A.

## Next: Task 5 (bench firmware), then Task 6 (calibration)
- `ShieldDriver` (ADR-009): Wire timeout checked after every write, 400 kHz.
- `RelayDriver` for today's chain relays.
- Bench serial menu at 115200:
  - hold-to-jog each axis (dead-man, like Space Ships `src/bench_test.cpp`);
  - stop-all;
  - switch watch: raw + filtered + glitches per second for all 10 limits;
  - max loop time.
- Purpose: before/after-cap glitch numbers, and the check on the merged supply.
- Write the logic test-first in `lib/motion` where possible; keep hardware code thin.
- Run `pio run -e joust -e bench` and `pio test -e native` after every change; report flash and RAM.
- Ask the owner before flashing. Task 5 is the first firmware that moves motors.

## Owner's hardware in progress (`tasks/todo.md` H1-H3)
- Caps: 0.1 uF across each motor. 330 uF + 0.1 uF at the supply input. 100 uF + 0.1 uF at the shield terminal and the DRV8871.
- Lance input filter: 1 kOhm pull-up, 1 kOhm series, 100 nF.
- Signal line repairs, brush checks, the merged supply, and the relay.
