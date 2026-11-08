#include <unity.h>
#include "speed_profile.h"

void setUp() {}
void tearDown() {}

static SpeedProfile lance() {
  SpeedProfile p;
  p.cruiseDuty = 2000;
  p.crawlDuty = 600;
  p.rampUpMs = 200;
  p.decel = true;
  p.decelStartMs = 1800;
  p.decelMs = 600;
  return p;
}

void test_constant_when_decel_off() {
  SpeedProfile p = lance();
  p.decel = false;
  p.rampUpMs = 0;
  for (uint32_t t = 0; t < 20000; t += 7) TEST_ASSERT_EQUAL_UINT16(2000, profileDuty(p, t));
}

void test_constant_profile_helper() {
  SpeedProfile p = constantProfile(800);
  TEST_ASSERT_EQUAL_UINT16(800, profileDuty(p, 0));
  TEST_ASSERT_EQUAL_UINT16(800, profileDuty(p, 100000));
}

void test_stage_values() {
  SpeedProfile p = lance();
  TEST_ASSERT_EQUAL_UINT16(600, profileDuty(p, 0));      // ramp starts at crawl
  TEST_ASSERT_EQUAL_UINT16(1300, profileDuty(p, 100));   // halfway up the linear ramp
  TEST_ASSERT_EQUAL_UINT16(2000, profileDuty(p, 200));   // cruise
  TEST_ASSERT_EQUAL_UINT16(2000, profileDuty(p, 1800));  // decel starts
  TEST_ASSERT_EQUAL_UINT16(1300, profileDuty(p, 2100));  // S-curve midpoint
  TEST_ASSERT_EQUAL_UINT16(600, profileDuty(p, 2400));   // crawl
  TEST_ASSERT_EQUAL_UINT16(600, profileDuty(p, 60000));  // crawl holds until stopped
}

void test_bounds_and_monotonic_ramps() {
  SpeedProfile p = lance();
  uint16_t prev = profileDuty(p, 0);
  for (uint32_t t = 1; t <= 3000; t++) {
    uint16_t d = profileDuty(p, t);
    TEST_ASSERT_TRUE(d >= p.crawlDuty && d <= p.cruiseDuty);
    if (t <= 200) TEST_ASSERT_TRUE(d >= prev);  // ramp up never drops
    if (t > 1800) TEST_ASSERT_TRUE(d <= prev);  // decel never rises
    // Continuous: no step bigger than the steepest slope allows (S-curve peak = 1.5x linear).
    TEST_ASSERT_TRUE((d > prev ? d - prev : prev - d) <= 8);
    prev = d;
  }
}

void test_decel_starting_inside_ramp_up_is_continuous() {
  SpeedProfile p = lance();
  p.decelStartMs = 100;  // before the ramp-up finishes
  uint16_t atStart = profileDuty(p, 100);
  TEST_ASSERT_EQUAL_UINT16(1300, atStart);
  TEST_ASSERT_TRUE(profileDuty(p, 101) <= atStart);
  TEST_ASSERT_EQUAL_UINT16(600, profileDuty(p, 700));
}

void test_crawl_above_cruise_is_clamped() {
  SpeedProfile p = lance();
  p.crawlDuty = 2500;  // misconfigured
  for (uint32_t t = 0; t < 3000; t += 10) TEST_ASSERT_TRUE(profileDuty(p, t) <= 2000);
}

void test_zero_length_stages() {
  SpeedProfile p = lance();
  p.rampUpMs = 0;
  p.decelMs = 0;
  TEST_ASSERT_EQUAL_UINT16(2000, profileDuty(p, 0));
  TEST_ASSERT_EQUAL_UINT16(2000, profileDuty(p, 1799));
  TEST_ASSERT_EQUAL_UINT16(600, profileDuty(p, 1800));
}

void test_duty_capped_at_12_bit() {
  SpeedProfile p = constantProfile(5000);
  TEST_ASSERT_EQUAL_UINT16(DUTY_MAX, profileDuty(p, 0));
}

int main() {
  UNITY_BEGIN();
  RUN_TEST(test_constant_when_decel_off);
  RUN_TEST(test_constant_profile_helper);
  RUN_TEST(test_stage_values);
  RUN_TEST(test_bounds_and_monotonic_ramps);
  RUN_TEST(test_decel_starting_inside_ramp_up_is_continuous);
  RUN_TEST(test_crawl_above_cruise_is_clamped);
  RUN_TEST(test_zero_length_stages);
  RUN_TEST(test_duty_capped_at_12_bit);
  return UNITY_END();
}
