#include "axis.h"

Axis::Axis(MotorDriver &driver, const AxisConfig &config)
    : driver_(driver),
      config_(config),
      limits_{SwitchFilter(config.integrateMs, config.maxStepMs),
              SwitchFilter(config.integrateMs, config.maxStepMs)} {}

void Axis::begin(bool rawA, bool rawB, uint32_t nowMs) {
  nowMs_ = nowMs;
  limits_[idx(End::A)].begin(rawA, nowMs);
  limits_[idx(End::B)].begin(rawB, nowMs);
  state_ = AxisState::IDLE;
  outcome_ = Outcome::NONE;
  haltMotor();
}

bool Axis::requestMove(End target, uint32_t nowMs) {
  if (state_ != AxisState::IDLE) return false;
  nowMs_ = nowMs;
  target_ = target;
  if (atLimit(target)) {
    outcome_ = Outcome::REACHED;
    return true;
  }
  const bool atOrigin = atLimit(opposite(target));
  spec_ = atOrigin ? &config_.fromEnd[idx(target)] : &config_.fromUnknown[idx(target)];
  originOpened_ = !atOrigin;
  startMs_ = nowMs;
  state_ = AxisState::MOVING;
  outcome_ = Outcome::NONE;
  duty_ = profileDuty(spec_->profile, 0);
  driver_.drive(target, duty_);
  return true;
}

void Axis::update(bool rawA, bool rawB, uint32_t nowMs) {
  nowMs_ = nowMs;
  limits_[idx(End::A)].update(rawA, nowMs);
  limits_[idx(End::B)].update(rawB, nowMs);

  if (atLimit(End::A) && atLimit(End::B)) {
    if (state_ != AxisState::FAULTED) finish(AxisState::FAULTED, Outcome::WIRING_FAULT);
  }
  if (state_ != AxisState::MOVING) {
    if (nowMs - lastStopMs_ >= config_.idleRestopMs) haltMotor();
    return;
  }

  const uint32_t elapsed = nowMs - startMs_;
  if (atLimit(target_) && elapsed >= spec_->minTravelMs) {
    finish(AxisState::IDLE, Outcome::REACHED);
    return;
  }
  // The limit behind the motor must open soon after start and never close again.
  if (atLimit(opposite(target_))) {
    if (originOpened_ || elapsed >= config_.originReleaseMs) {
      finish(AxisState::FAULTED, Outcome::STUCK_OR_REVERSED);
      return;
    }
  } else {
    originOpened_ = true;
  }
  if (elapsed >= spec_->cutoffMs) {
    finish(AxisState::FAULTED, Outcome::CUTOFF);
    return;
  }

  const uint16_t d = profileDuty(spec_->profile, elapsed);
  if (d != duty_) {
    duty_ = d;
    driver_.drive(target_, duty_);
  }
}

void Axis::stop() {
  if (state_ == AxisState::MOVING) {
    finish(AxisState::IDLE, Outcome::STOPPED);
  } else {
    haltMotor();
  }
}

void Axis::clearFault() {
  if (state_ != AxisState::FAULTED) return;
  state_ = AxisState::IDLE;
  outcome_ = Outcome::NONE;
}

void Axis::haltMotor() {
  driver_.stop();
  duty_ = 0;
  lastStopMs_ = nowMs_;
}

// Every way out of a move goes through here, so the motor is always stopped.
void Axis::finish(AxisState next, Outcome why) {
  haltMotor();
  state_ = next;
  outcome_ = why;
}
