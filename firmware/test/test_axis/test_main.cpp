#include <unity.h>
#include "axis.h"

struct FakeDriver : MotorDriver {
  bool running = false;
  End dir = End::A;
  uint16_t duty = 0;
  int drives = 0;
  int stops = 0;
  void drive(End toward, uint16_t d) override {
    running = true;
    dir = toward;
    duty = d;
    drives++;
  }
  void stop() override {
    running = false;
    duty = 0;
    stops++;
  }
};

static AxisConfig makeConfig() {
  AxisConfig c = {};
  for (int i = 0; i < 2; i++) {
    SpeedProfile p = {};
    p.cruiseDuty = 2000;
    p.crawlDuty = 600;
    p.decel = true;
    p.decelStartMs = 1500;
    p.decelMs = 300;
    c.fromEnd[i].profile = p;
    c.fromEnd[i].cutoffMs = 3000;
    c.fromEnd[i].minTravelMs = 200;
    c.fromUnknown[i].profile = constantProfile(600);
    c.fromUnknown[i].cutoffMs = 8000;
  }
  c.originReleaseMs = 500;
  c.integrateMs = 12;
  c.maxStepMs = 4;
  c.idleRestopMs = 100;
  return c;
}

static AxisConfig cfg;
static FakeDriver drv;
static Axis *axis;
static uint32_t now;
static bool rawA, rawB;

static Axis &setUpAxis(bool a, bool b) {
  cfg = makeConfig();
  drv = FakeDriver();
  static Axis *held = nullptr;
  delete held;
  held = new Axis(drv, cfg);
  axis = held;
  now = 1000;
  rawA = a;
  rawB = b;
  axis->begin(a, b, now);
  return *axis;
}

// Advances time 1 ms per update until `ms` pass or the axis stops moving.
static void runFor(uint32_t ms) {
  for (uint32_t i = 0; i < ms; i++) {
    now++;
    axis->update(rawA, rawB, now);
    if (axis->state() != AxisState::MOVING) return;
  }
}

void setUp() {}
void tearDown() {}

void test_reached_stops_motor_same_tick() {
  Axis &ax = setUpAxis(true, false);  // at A (lance up)
  TEST_ASSERT_TRUE(ax.requestMove(End::B, now));
  TEST_ASSERT_TRUE(drv.running);
  TEST_ASSERT_TRUE(drv.dir == End::B);
  rawA = false;  // origin opens
  runFor(1000);
  rawB = true;  // arrives
  for (int i = 0; i < 100 && ax.state() == AxisState::MOVING; i++) {
    now++;
    ax.update(rawA, rawB, now);
    if (ax.limit(End::B).changed()) TEST_ASSERT_FALSE(drv.running);
  }
  TEST_ASSERT_TRUE(ax.outcome() == Outcome::REACHED);
  TEST_ASSERT_TRUE(ax.state() == AxisState::IDLE);
  TEST_ASSERT_FALSE(drv.running);
}

// Regression: old firmware set "done" on lance timeout but never stopped the motor.
void test_cutoff_stops_motor_and_faults() {
  Axis &ax = setUpAxis(true, false);
  ax.requestMove(End::B, now);
  rawA = false;
  runFor(5000);
  TEST_ASSERT_TRUE(ax.outcome() == Outcome::CUTOFF);
  TEST_ASSERT_TRUE(ax.state() == AxisState::FAULTED);
  TEST_ASSERT_FALSE(drv.running);
  TEST_ASSERT_EQUAL_UINT32(1000 + 3000, now);
}

void test_faulted_axis_rejects_moves_until_cleared() {
  Axis &ax = setUpAxis(true, false);
  ax.requestMove(End::B, now);
  rawA = false;
  runFor(5000);
  int drivesBefore = drv.drives;
  TEST_ASSERT_FALSE(ax.requestMove(End::A, now));
  TEST_ASSERT_EQUAL(drivesBefore, drv.drives);
  ax.clearFault();
  TEST_ASSERT_TRUE(ax.state() == AxisState::IDLE);
  TEST_ASSERT_TRUE(ax.requestMove(End::A, now));
}

// Regression: old firmware re-read the raw pin after starting; one noisy read = never watched.
void test_glitch_on_target_after_start_does_not_end_move() {
  Axis &ax = setUpAxis(true, false);
  ax.requestMove(End::B, now);
  rawA = false;
  runFor(5);
  rawB = true;  // 3 ms spike
  runFor(3);
  rawB = false;
  runFor(50);
  TEST_ASSERT_TRUE(ax.state() == AxisState::MOVING);
  TEST_ASSERT_TRUE(drv.running);
}

void test_sustained_false_closure_before_min_travel_ignored() {
  Axis &ax = setUpAxis(true, false);
  ax.requestMove(End::B, now);
  rawA = false;
  runFor(50);
  rawB = true;  // 30 ms: long enough to pass the filter, but before minTravel (200 ms)
  runFor(30);
  TEST_ASSERT_TRUE(ax.atLimit(End::B));
  rawB = false;
  runFor(100);
  TEST_ASSERT_TRUE(ax.state() == AxisState::MOVING);
  rawB = true;  // real arrival
  runFor(1000);
  TEST_ASSERT_TRUE(ax.outcome() == Outcome::REACHED);
}

void test_origin_never_opens_is_stuck() {
  Axis &ax = setUpAxis(true, false);
  ax.requestMove(End::B, now);  // rawA stays closed: stuck motor or wrong direction
  runFor(5000);
  TEST_ASSERT_TRUE(ax.outcome() == Outcome::STUCK_OR_REVERSED);
  TEST_ASSERT_TRUE(ax.state() == AxisState::FAULTED);
  TEST_ASSERT_FALSE(drv.running);
  TEST_ASSERT_EQUAL_UINT32(1000 + 500, now);
}

void test_limit_behind_motor_closing_is_reversed() {
  Axis &ax = setUpAxis(false, false);  // unknown position
  ax.requestMove(End::B, now);
  runFor(100);
  rawA = true;  // ran the wrong way into A
  runFor(100);
  TEST_ASSERT_TRUE(ax.outcome() == Outcome::STUCK_OR_REVERSED);
  TEST_ASSERT_FALSE(drv.running);
}

void test_both_limits_closed_is_wiring_fault_while_moving() {
  Axis &ax = setUpAxis(true, false);
  ax.requestMove(End::B, now);
  rawB = true;  // A still closed too
  runFor(100);
  TEST_ASSERT_TRUE(ax.outcome() == Outcome::WIRING_FAULT);
  TEST_ASSERT_FALSE(drv.running);
}

void test_both_limits_closed_is_wiring_fault_while_idle() {
  Axis &ax = setUpAxis(true, false);
  rawB = true;
  for (int i = 0; i < 50; i++) ax.update(rawA, rawB, ++now);
  TEST_ASSERT_TRUE(ax.state() == AxisState::FAULTED);
  TEST_ASSERT_TRUE(ax.outcome() == Outcome::WIRING_FAULT);
}

void test_already_at_target_does_not_move() {
  Axis &ax = setUpAxis(false, true);
  TEST_ASSERT_TRUE(ax.requestMove(End::B, now));
  TEST_ASSERT_TRUE(ax.outcome() == Outcome::REACHED);
  TEST_ASSERT_TRUE(ax.state() == AxisState::IDLE);
  TEST_ASSERT_EQUAL(0, drv.drives);
}

void test_rejects_move_while_moving() {
  Axis &ax = setUpAxis(true, false);
  ax.requestMove(End::B, now);
  TEST_ASSERT_FALSE(ax.requestMove(End::A, now));
  TEST_ASSERT_TRUE(ax.target() == End::B);
}

void test_from_unknown_uses_homing_spec() {
  Axis &ax = setUpAxis(false, false);
  ax.requestMove(End::B, now);
  TEST_ASSERT_EQUAL_UINT16(600, drv.duty);
  runFor(3500);  // past the from-end cutoff (3000)
  TEST_ASSERT_TRUE(ax.state() == AxisState::MOVING);
  runFor(10000);
  TEST_ASSERT_TRUE(ax.outcome() == Outcome::CUTOFF);
  TEST_ASSERT_EQUAL_UINT32(1000 + 8000, now);
}

void test_duty_follows_profile_and_writes_only_on_change() {
  Axis &ax = setUpAxis(true, false);
  ax.requestMove(End::B, now);
  rawA = false;
  TEST_ASSERT_EQUAL_UINT16(2000, drv.duty);
  runFor(1400);  // cruise: no new writes
  TEST_ASSERT_EQUAL(1, drv.drives);
  runFor(500);  // through decel
  TEST_ASSERT_EQUAL_UINT16(600, drv.duty);
  TEST_ASSERT_EQUAL_UINT16(600, ax.duty());
  TEST_ASSERT_TRUE(drv.drives > 1 && drv.drives <= 301);
}

void test_idle_axis_resends_stop() {
  Axis &ax = setUpAxis(true, false);
  int before = drv.stops;
  for (int i = 0; i < 1000; i++) ax.update(rawA, rawB, ++now);
  TEST_ASSERT_INT_WITHIN(1, 10, drv.stops - before);
}

void test_stop_mid_move() {
  Axis &ax = setUpAxis(true, false);
  ax.requestMove(End::B, now);
  rawA = false;
  runFor(300);
  ax.stop();
  TEST_ASSERT_TRUE(ax.outcome() == Outcome::STOPPED);
  TEST_ASSERT_TRUE(ax.state() == AxisState::IDLE);
  TEST_ASSERT_FALSE(drv.running);
}

int main() {
  UNITY_BEGIN();
  RUN_TEST(test_reached_stops_motor_same_tick);
  RUN_TEST(test_cutoff_stops_motor_and_faults);
  RUN_TEST(test_faulted_axis_rejects_moves_until_cleared);
  RUN_TEST(test_glitch_on_target_after_start_does_not_end_move);
  RUN_TEST(test_sustained_false_closure_before_min_travel_ignored);
  RUN_TEST(test_origin_never_opens_is_stuck);
  RUN_TEST(test_limit_behind_motor_closing_is_reversed);
  RUN_TEST(test_both_limits_closed_is_wiring_fault_while_moving);
  RUN_TEST(test_both_limits_closed_is_wiring_fault_while_idle);
  RUN_TEST(test_already_at_target_does_not_move);
  RUN_TEST(test_rejects_move_while_moving);
  RUN_TEST(test_from_unknown_uses_homing_spec);
  RUN_TEST(test_duty_follows_profile_and_writes_only_on_change);
  RUN_TEST(test_idle_axis_resends_stop);
  RUN_TEST(test_stop_mid_move);
  return UNITY_END();
}
