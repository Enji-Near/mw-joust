# Handoff: Joust v2 wiring diagram

Start a new session in the Joust repo and say: "Read `docs/handoff-wiring-diagram.md` and follow it."

## Goal
Draw the wiring for the Joust v2 installation: power, motors, limit switches, control panel, indicator, motor-power relay, and the future DRV8871 and ESP32. Audience: the owner (builds and maintains it) and on-site staff (troubleshooting). Output goes in `docs/wiring/`.

Before drawing, ask the owner which format they want. Options:
- One-page block diagram (power and signal paths) as SVG or Mermaid, plus a pin table. Recommended first step: fast, easy to review.
- WireViz (YAML -> harness diagram + parts list; https://github.com/wireviz/WireViz) for the connector-level harnesses.
- KiCad schematic, if a protoboard (lance input filters) gets built.

## Read first
1. `CLAUDE.md` (project rules: public repo, commit and push every turn on branch `firmware-v2`, cite datasheets, never invent hardware facts).
2. `firmware/include/config.h`: **the source of truth for pins.** If the diagram needs a pin change, change config.h and run `pio run -e joust -e bench` from `firmware/`.
3. `tasks/plan.md`: ADR-007 (DRV8871), ADR-010 (fault policy), ADR-011 (motor-power relay), ADR-012 (indicator), and the Power paragraph.
4. `tasks/todo.md` H1-H4: hardware changes in progress (the owner ticks items there; re-read before editing).
5. v1 reference: `2025_04_02_Joust_Deployment_NRG/*.ino` and `Electronics Pinout.xlsx` (the owner's v1 pinout; reading it needs `openpyxl`, or ask the owner for a CSV export).

## System as known (2026-11-07)

**Controller:** Arduino Mega 2560, with an Adafruit Motor Shield V2 stacked on top (it covers the Mega and its LEDs).

**Power:**
- Two separate 12 V supplies: #1 feeds the shield's motor terminal, #2 feeds the chain drive. They were split in v1 to keep chain motor noise off the shield, and stay split for now.
- Fuse each supply at its + output: 3 A time-lag (the legacy value).
- Caps: 330 uF + 0.1 uF at each supply input, before the relay. 100 uF + 0.1 uF at the shield motor terminal and at the DRV8871 VM, after the relay. Electrolytics rated 25 V or higher.
- 0.1 uF ceramic directly across each motor's terminals. **Never an electrolytic across a motor.** Motor leads twisted.
- All grounds meet at one point. The DRV8871 inputs are referenced to Mega GND.

**Motors (all 12 V DC):**

| Motor | Driver | Notes |
| --- | --- | --- |
| K1 lance | Shield M1 | down = FORWARD |
| K1 rotate | Shield M2 | CW = BACKWARD |
| K2 lance | Shield M3 | down = FORWARD |
| K2 rotate | Shield M4 | CW = BACKWARD |
| Chain | Today: relays on A0 (CCW) and A1 (CW), active LOW. v2: Adafruit DRV8871 (product 3190), IN1/IN2 on D44/D45 | Which input drives CW is set on the bench. Board current limit about 2.1 A with its 30 kOhm ILIM resistor (DRV8871 datasheet 7.3.3). |

**Inputs:** all switch to GND, active LOW, with a pull-up.
- Limits:
  - D3 K1 lance down [B']
  - D4 K2 lance down [B]
  - D5 chain CCW [A]
  - D6 chain CW [A']
  - D7 K1 lance up [C']
  - D8 K2 lance up [C]
  - D9 K1 rotate CCW [D']
  - D10 K2 rotate CCW [D]
  - D11 K1 rotate CW [E']
  - D12 K2 rotate CW [E]
- Lance switch construction: a bolt on the axle touches a post on a small PCB; the bolt is grounded through a brush.
- Recommended lance input circuit, at the Mega: 1 kOhm pull-up to 5 V, then 1 kOhm in series, then 100 nF to GND at the pin. Ask the owner whether the other six limits get the same circuit.
- Start button D2; key A9; safe-op jumper A12.
- Control panel: A2 both rotate CW, A3 both rotate CCW, A10 chain CW, A11 chain CCW. Both lances up and down move from A4/A5 (removed) to D23 and D24 (tentative).

**Outputs:**
- D13: onboard LED. v1 code calls it the start-button LED. Ask whether an external LED is wired to it.
- D22: motor-power cut-off relay (tentative). 2-relay board as a placeholder, then a single-relay board. The relay must be OFF while the pin floats (reset, upload).
- D25: fault indicator, NeoPixel strip (proposed, owner undecided): 11+ pixels, about 470 ohm in series on the data line, 470-1000 uF across the strip's 5 V at the strip (Adafruit NeoPixel Uberguide). Brightness is capped in firmware.

**Reserved pins:** don't assign these.
- D0/D1: USB serial.
- D18/D19: Serial1, for the future ESP32.
- D20/D21: Mega I2C (SDA/SCL), also on the R3 header the shield uses.
- D44-D46: Timer5 (DRV8871 PWM).
- D50-D53: SPI. Free, but keep them spare.

**Future ESP32 (stretch S1):**
- Connects to Serial1 (Mega TX1 D18 -> ESP32 RX, ESP32 TX -> Mega RX1 D19).
- The ESP32 is 3.3 V, so the link needs a level shifter. Mega 5 V into an ESP32 pin is over its rating. 3.3 V into the Mega is marginal: check VIH in the ATmega2560 datasheet's DC characteristics. Mark it as future work on the diagram.

## Open items to ask the owner (don't guess)
1. **Motor-power relay with two supplies:** one relay contact cuts one supply. Options:
   - use both channels of the 2-relay board, both driven from D22;
   - a DPDT relay;
   - cut only the shield supply and rely on the DRV8871 inputs going low (coast) at reset.

   Present the trade-off and let the owner choose.
2. Which supply powers the Mega itself (USB, barrel jack, or VIN from a 12 V supply)?
3. The current rating of each 12 V supply, and the wire gauges in use.
4. Where the panel, key, start button and indicator physically sit, and which connectors or terminals each harness uses.
5. Whether the shield ties A4/A5 to SDA/SCL. The v1 code says so; it's unverified on the Mega. Owner check, with power off: continuity between the shield's A4 and SDA pins.
6. Indicator: NeoPixel or discrete LEDs, and where staff will see it.

## Done when
- The diagram shows every pin in `config.h` and every item above, with unknowns marked "unknown" or "TBD".
- The owner has reviewed it and the open items are answered or listed on it.
- The files are committed and pushed on `firmware-v2`.
