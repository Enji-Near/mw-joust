// One motor between two limit switches (ADR-002, ADR-008). Non-blocking: call update() every loop.
#pragma once
#include <stdint.h>
#include "speed_profile.h"
#include "switch_filter.h"

// The two ends of an axis. Lance: A = up, B = down. Rotate and chain: A = CW, B = CCW.
enum class End : uint8_t { A = 0, B = 1 };
inline End opposite(End e) { return e == End::A ? End::B : End::A; }

// Hardware backend: Adafruit shield port, relay pair, or DRV8871.
class MotorDriver {
 public:
  virtual void drive(End toward, uint16_t duty) = 0;  // duty 0..DUTY_MAX
  virtual void stop() = 0;

 protected:
  ~MotorDriver() {}
};

struct MoveSpec {
  SpeedProfile profile;
  uint32_t cutoffMs;     // target not reached by then: stop, CUTOFF
  uint32_t minTravelMs;  // target closures earlier than this are ignored (0 = off)
};

struct AxisConfig {
  MoveSpec fromEnd[2];       // [target]: move starts at the opposite end
  MoveSpec fromUnknown[2];   // [target]: neither limit closed at start (homing)
  uint32_t originReleaseMs;  // the limit the move started on must open within this
  uint16_t integrateMs;      // switch filter, see SwitchFilter
  uint16_t maxStepMs;
  uint16_t idleRestopMs;     // re-send stop this often while not moving (lost I2C writes)
};

enum class AxisState : uint8_t { IDLE, MOVING, FAULTED };

enum class Outcome : uint8_t {
  NONE,
  REACHED,            // target limit closed
  STOPPED,            // stop() called mid-move
  CUTOFF,             // fault: target not reached in time
  STUCK_OR_REVERSED,  // fault: origin never opened, or a limit behind the motor closed
  WIRING_FAULT,       // fault: both limits closed at once
};

class Axis {
 public:
  Axis(MotorDriver &driver, const AxisConfig &config);

  void begin(bool rawA, bool rawB, uint32_t nowMs);   // seed the filters from raw reads
  void update(bool rawA, bool rawB, uint32_t nowMs);  // raw = input is active (switch closed)

  // Starts a move to `target`. Already there: REACHED at once, no motion.
  // Returns false (and does nothing) while moving or faulted.
  bool requestMove(End target, uint32_t nowMs);
  void stop();          // abort a move; the motor is always stopped
  void clearFault();    // operator unlock only

  AxisState state() const { return state_; }
  Outcome outcome() const { return outcome_; }  // last move result or fault
  bool atLimit(End e) const { return limits_[idx(e)].active(); }
  const SwitchFilter &limit(End e) const { return limits_[idx(e)]; }
  End target() const { return target_; }
  uint32_t moveStartMs() const { return startMs_; }
  uint16_t duty() const { return duty_; }

 private:
  static uint8_t idx(End e) { return (uint8_t)e; }
  void haltMotor();
  void finish(AxisState next, Outcome why);

  MotorDriver &driver_;
  const AxisConfig &config_;
  SwitchFilter limits_[2];
  AxisState state_ = AxisState::IDLE;
  Outcome outcome_ = Outcome::NONE;
  const MoveSpec *spec_ = nullptr;
  End target_ = End::A;
  uint32_t startMs_ = 0;
  uint32_t nowMs_ = 0;
  uint32_t lastStopMs_ = 0;
  uint16_t duty_ = 0;
  bool originOpened_ = false;
};
