# CLAUDE.md — Joust (Miniature World, Camelot)

## Project
- Two knights on a chain drive joust past each other: lances up/down, knights rotate, chain drive carries them. Unattended public display, heavy daily use.
- Live firmware v1: `2025_04_02_Joust_Deployment_NRG/` (Arduino sketch, reference only). v2: `firmware/` (PlatformIO). Plan and tasks: `tasks/plan.md`, `tasks/todo.md`.
- `motor_bench_test/`: earlier N20/stepper retrofit endurance test. Retrofit set aside: existing lance motors and shield stay.

## Hardware
- Board: Arduino Mega 2560. Flash 256 KB, SRAM 8 KB, EEPROM 4 KB (ATmega2560 datasheet).
- Adafruit Motor Shield V2 (PCA9685 + TB6612, I2C 0x60): M1 K1 lance, M2 K1 rotate, M3 K2 lance, M4 K2 rotate. Library 1.1.4: no BRAKE, RELEASE = coast.
- Chain drive: two relays (A0 CCW, A1 CW, active LOW) today; DRV8871 on D44/D45 planned (Task 9).
- All motors 12 V. Chain at full 12 V; lance and rotate PWM'd down (old duties 800 and 2400 of 4095).
- Limit switches: bolt on the axle touching a post on a small PCB, bolt grounded through a brush. Fragile, the post is also the hard stop. All inputs to GND with pull-up, active LOW.
- Pins: all in `firmware/include/config.h`. Control panel (manual mode) retired; A4/A5 share the shield's I2C lines.

## Toolchain and commands
- From `firmware/`: `pio run -e joust -e bench` (Mega builds), `pio test -e native` (host unit tests, Unity).
- `lib/motion`: SwitchFilter, SpeedProfile, Axis. Pure C++ (no Arduino.h), shared by both envs and tested on the host.

## Code standards
- Every tunable (pins, polarity, timings, speeds, calibration) lives in `firmware/include/config.h`.
- Motion logic goes in `lib/motion` with host tests, written test-first. Hardware code stays thin.
- Every motor move goes through `Axis`; every exit path stops the motor.
- No `delay()` in the live loop. `F()` on string literals. Serial 115200.
- Check every bus transaction and report failures.
- Comments explain why, in one short line where possible.
- Pin every library version.

## Workflow rules
- Direct language, bullets, minimal explanation unless asked.
- Run `pio run` and `pio test -e native` after every change; fix warnings; report flash and RAM.
- Never upload, or run anything that moves hardware, without asking first.
- Commit and push only when asked.
- Smallest solution that works. No adaptive or learning behaviour.
- Don't add behaviour that wasn't asked for; propose extra safety checks and wait.
- No secrets or credentials in tracked files (public repo). The owner's work phone and email are public and belong in the client operations manual.

## Verification rules
- Cite datasheets or library source (file:line) for hardware and library facts; otherwise mark "unverified".
- Bench values (directions, travel times, currents) come from the owner.
