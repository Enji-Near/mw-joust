// Open-loop, time-based speed profile for one move (ADR-004).
#pragma once
#include <stdint.h>

const uint16_t DUTY_MAX = 4095;  // 12-bit, matches Adafruit setSpeedFine()

// crawl -> (linear ramp-up) -> cruise -> (S-curve decel) -> crawl until the limit stops the move.
struct SpeedProfile {
  uint16_t cruiseDuty;
  uint16_t crawlDuty;     // slowest duty that still moves; ramp-up starts here
  uint16_t rampUpMs;      // 0 = start straight at cruise
  bool decel;             // false = hold cruise until stopped (old firmware behaviour)
  uint32_t decelStartMs;  // from move start; set from the fastest calibrated trip
  uint16_t decelMs;       // 0 = drop straight to crawl
};

// Duty to apply `elapsedMs` after the move started. Never above cruise, never below crawl.
uint16_t profileDuty(const SpeedProfile &p, uint32_t elapsedMs);

// A profile that holds one duty, e.g. crawling home from an unknown position.
SpeedProfile constantProfile(uint16_t duty);
