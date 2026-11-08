// Joust firmware v2: every pin, polarity, timing and tunable value.
// Pins and old timings ported from 2025_04_02_Joust_Deployment_NRG.ino (lines 9-50).
// Calibration values are measured with the bench env (Task 6) and pasted here.
#pragma once
#include <Arduino.h>

// ---------------------------------------------------------------- Inputs
// All inputs: switch to GND, INPUT_PULLUP, so active = LOW.
const uint8_t INPUT_ACTIVE_LEVEL = LOW;

const uint8_t PIN_START_BUTTON = 2;     // [START]
const uint8_t PIN_UNLOCK_KEY   = A9;    // keyed lock; unlock = a lock->unlock change of state
const uint8_t PIN_SAFE_OP      = A12;   // jumper to GND = safe-operation mode

// Limit switches. [x] = label on the control PCB.
const uint8_t PIN_K1_LANCE_DOWN = 3;    // [B']
const uint8_t PIN_K2_LANCE_DOWN = 4;    // [B ]
const uint8_t PIN_CHAIN_CCW     = 5;    // [A ]
const uint8_t PIN_CHAIN_CW      = 6;    // [A']
const uint8_t PIN_K1_LANCE_UP   = 7;    // [C']
const uint8_t PIN_K2_LANCE_UP   = 8;    // [C ]
const uint8_t PIN_K1_ROTATE_CCW = 9;    // [D']
const uint8_t PIN_K2_ROTATE_CCW = 10;   // [D ]
const uint8_t PIN_K1_ROTATE_CW  = 11;   // [E']
const uint8_t PIN_K2_ROTATE_CW  = 12;   // [E ]

// Control panel (manual mode), kept until v2 is proven reliable.
const uint8_t PIN_PANEL_ROTATE_CW  = A2;
const uint8_t PIN_PANEL_ROTATE_CCW = A3;
const uint8_t PIN_PANEL_CHAIN_CW   = A10;
const uint8_t PIN_PANEL_CHAIN_CCW  = A11;
// Both-lances buttons moved off A4/A5 (old sketch warned they share the shield's I2C;
// unverified on the Mega, whose I2C is D20/D21). Tentative pins, not wired yet.
const uint8_t PIN_PANEL_LANCES_UP   = 23;   // was A4
const uint8_t PIN_PANEL_LANCES_DOWN = 24;   // was A5

// ---------------------------------------------------------------- Outputs
const uint8_t PIN_LED = 13;             // onboard LED, mirrors the start-button LED

// Fault indicator (proposed): NeoPixel strip, one pixel per limit switch + status. Tentative pin.
const uint8_t PIN_STATUS_PIXELS = 25;

// Chain drive relays, active LOW. Replaced by the DRV8871 in Task 9.
const uint8_t PIN_CHAIN_RELAY_CCW = A0;
const uint8_t PIN_CHAIN_RELAY_CW  = A1;
const uint8_t RELAY_ON_LEVEL      = LOW;
const uint8_t RELAY_OFF_LEVEL     = HIGH;

// Motor-power cut-off (ADR-011). 2-relay board as placeholder, then a single-relay board.
// Must be OFF while the pin floats (reset, bootloader, upload).
const uint8_t PIN_MOTOR_POWER = 22;

// ---------------------------------------------------------------- Motor shield
// Adafruit Motor Shield V2, default I2C address 0x60. Ports M1..M4.
const uint8_t SHIELD_PORT_K1_LANCE  = 1;
const uint8_t SHIELD_PORT_K1_ROTATE = 2;
const uint8_t SHIELD_PORT_K2_LANCE  = 3;
const uint8_t SHIELD_PORT_K2_ROTATE = 4;

// Direction mapping, from the old sketch: lance down = FORWARD, rotate CW = BACKWARD.
const bool LANCE_DOWN_IS_FORWARD = true;
const bool ROTATE_CW_IS_FORWARD  = false;

// Duty is 12-bit (0..4095, setSpeedFine). Old setSpeed(s) wrote s * 16
// (Adafruit_MotorShield.cpp:255), so these match the old speeds exactly.
const uint16_t DUTY_LANCE_OLD  = 50 * 16;    // 800  = 19.5 %
const uint16_t DUTY_ROTATE_OLD = 150 * 16;   // 2400 = 58.6 %

// ---------------------------------------------------------------- Timing
// Old cutoffs (ms). Replaced per axis and direction by calibration (Task 6).
const uint32_t CUTOFF_LANCE_OLD_MS  = 3000;   // lanceMillisPeriod; old comment: lance trip = 3000
const uint32_t CUTOFF_CHAIN_OLD_MS  = 9600;   // old comment: chain trip avg = 9300
const uint32_t CUTOFF_ROTATE_OLD_MS = 31000;  // old comment: rotation trip = 30500

// Switch filter (ADR-003): ms of accumulated active time to flip, and the most
// one sample may contribute (so a single read after a stalled loop can't flip it).
const uint16_t LIMIT_INTEGRATE_MS  = 12;
const uint16_t BUTTON_INTEGRATE_MS = 30;
const uint16_t FILTER_MAX_STEP_MS  = 4;

// Idle axes are re-sent STOP this often, in case an I2C write was lost (ADR-009).
const uint16_t IDLE_RESTOP_MS = 100;

const uint32_t SERIAL_BAUD = 115200;
