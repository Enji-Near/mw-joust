// Every tunable value for the bench test lives here.
#pragma once
#include <Arduino.h>

// --- N20 gearmotors: one DRV8833 module each. D3/D5/D9/D10 are PWM-capable. ---
const uint8_t N20_1_IN1 = 9, N20_1_IN2 = 10, N20_1_ENC_A = 2, N20_1_ENC_B = 4;
const uint8_t N20_2_IN1 = 3, N20_2_IN2 = 5,  N20_2_ENC_A = 7, N20_2_ENC_B = 8;

// --- 8 mm steppers: one DRV8833 module each. Order: AIN1, AIN2, BIN1, BIN2. ---
const uint8_t STEP_1_PINS[4] = {14, 15, 16, 17};  // A0-A3
const uint8_t STEP_2_PINS[4] = {18, 19, 20, 21};  // A4-A7

// Optional nFAULT inputs (open drain). Harmless if left unconnected.
const uint8_t FAULT_N20_PIN = 6, FAULT_STEP_PIN = 11;

// --- Test timing ---
const uint32_t MOVE_MS  = 1000;  // Each move takes this long.
const uint32_t DWELL_MS = 500;   // Pause at each end. Stepper coils are off during it, to cool.

// --- Steppers ---
// 3 full steps = 54 deg, assuming 18 deg/step (NOT in the listing: confirm with `t 1 20`).
// 45 deg would be 5 half-steps, but that leaves one end between detents, so the rotor
// would jump when the coils release for the dwell and the flag marks would mean nothing.
const uint8_t  STEPPER_STEPS_PER_MOVE = 3;
// false: one coil at a time, about 128 mA and 0.64 W per motor at 5 V.
// true:  two coils, double the heat and rail current, more torque.
const bool     STEPPER_TWO_COIL = false;
const uint32_t STEPPER_JOG_MS   = 200;  // Step interval for the `t` calibration command.

// --- N20 ---
// PLACEHOLDER: encoder resolution is not in the listing. 8344 assumes 7 PPR x 4 edges x 298:1.
// Measure it with `c` (see README) and replace this before marking the flags.
const int32_t N20_COUNTS_PER_REV = 8344;
const int32_t N20_MOVE_DEG       = 45;
const int32_t N20_TRAVEL_COUNTS  = N20_COUNTS_PER_REV * N20_MOVE_DEG / 360;
// Swap to true if a positive PWM makes the count go down (`c` reports this).
const bool N20_1_ENC_INVERT = false;
const bool N20_2_ENC_INVERT = false;

// Position loop. Starting values, not yet tuned on hardware.
const uint32_t N20_CONTROL_MS = 5;
const float    N20_KP = 0.5f;       // PWM per count of error
const float    N20_KI = 0.02f;      // PWM per count, per control tick
const float    N20_I_MAX = 100.0f;  // Integral limit, in PWM units
const int16_t  N20_MIN_PWM = 30;    // Added to overcome stiction at low speed
const int32_t  N20_TOL_COUNTS = 3;  // Close enough: brake rather than hunt
const int32_t  N20_STALL_COUNTS = N20_TRAVEL_COUNTS / 4;  // End-of-move error that counts as a stall
const int32_t  N20_RUNAWAY_COUNTS = N20_TRAVEL_COUNTS * 3;  // Reversed encoder: stop that motor
const uint32_t N20_JOG_TIMEOUT_MS = 60000UL;

// --- Persistence ---
const uint32_t CHECKPOINT_EVERY_CYCLES = 100;  // EEPROM wear vs cycles lost on a reset

const uint32_t SERIAL_BAUD = 115200;
