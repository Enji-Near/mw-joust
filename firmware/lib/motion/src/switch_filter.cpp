#include "switch_filter.h"

SwitchFilter::SwitchFilter(uint16_t integrateMs, uint16_t maxStepMs)
    : integrateMs_(integrateMs), maxStepMs_(maxStepMs) {}

void SwitchFilter::begin(bool rawActive, uint32_t nowMs) {
  state_ = rawActive;
  acc_ = rawActive ? integrateMs_ : 0;
  changed_ = false;
  lastMs_ = nowMs;
  onsetMs_ = nowMs;
  glitches_ = 0;
}

void SwitchFilter::update(bool rawActive, uint32_t nowMs) {
  uint32_t dt = nowMs - lastMs_;  // unsigned: correct across millis() wraparound
  lastMs_ = nowMs;
  if (dt > maxStepMs_) dt = maxStepMs_;
  changed_ = false;

  // The rail matching the current state, and the one that flips it.
  const uint16_t home = state_ ? integrateMs_ : 0;
  const uint16_t away = state_ ? 0 : integrateMs_;
  const bool wasHome = (acc_ == home);

  if (rawActive) {
    acc_ = (acc_ + dt >= integrateMs_) ? integrateMs_ : (uint16_t)(acc_ + dt);
  } else {
    acc_ = (acc_ <= dt) ? 0 : (uint16_t)(acc_ - dt);
  }

  if (wasHome && acc_ != home) excursionStartMs_ = nowMs;
  if (acc_ == away) {
    state_ = !state_;
    changed_ = true;
    onsetMs_ = excursionStartMs_;
  } else if (!wasHome && acc_ == home) {
    glitches_++;
  }
}
