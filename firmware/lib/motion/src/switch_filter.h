// Integrating debounce for one switch input (ADR-003). Replaces ezButton.
#pragma once
#include <stdint.h>

// Accumulates the time the raw reading is active vs inactive, clamped to
// 0..integrateMs. The filtered state flips only when the total reaches the far
// rail, so scattered noise samples cancel out instead of restarting a timer.
class SwitchFilter {
 public:
  // integrateMs: accumulated active (or inactive) time needed to flip.
  // maxStepMs: the most time one sample may add, so a single read after a
  // stalled loop pass can't flip the state on its own.
  SwitchFilter(uint16_t integrateMs, uint16_t maxStepMs);

  // Seed from a raw read at boot: the filter starts settled in that state.
  void begin(bool rawActive, uint32_t nowMs);
  void update(bool rawActive, uint32_t nowMs);

  bool active() const { return state_; }
  bool changed() const { return changed_; }  // flipped on the last update()

  // When the excursion that caused the last flip began (first raw sample
  // toward it). Used by calibration as the arrival / release time.
  uint32_t onsetMs() const { return onsetMs_; }

  // Excursions that returned without flipping (noise bursts) since clearStats().
  uint16_t glitches() const { return glitches_; }
  void clearStats() { glitches_ = 0; }

 private:
  uint16_t integrateMs_;
  uint16_t maxStepMs_;
  uint16_t acc_ = 0;
  bool state_ = false;
  bool changed_ = false;
  uint32_t lastMs_ = 0;
  uint32_t excursionStartMs_ = 0;
  uint32_t onsetMs_ = 0;
  uint16_t glitches_ = 0;
};
