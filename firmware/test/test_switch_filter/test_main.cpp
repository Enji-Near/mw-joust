#include <unity.h>
#include "switch_filter.h"

static const uint16_t LIMIT = 12;
static const uint16_t STEP = 4;

void setUp() {}
void tearDown() {}

// Deterministic noise source, so failures reproduce.
static uint32_t rng = 1;
static uint32_t nextRand() {
  rng = rng * 1103515245u + 12345u;
  return (rng >> 16) & 0x7FFF;
}

// Feeds `raw` every `stepMs` until the filter flips or `maxMs` passes. Returns flip time or 0.
static uint32_t runUntilFlip(SwitchFilter &f, uint32_t &t, uint32_t stepMs, uint32_t maxMs,
                             bool (*raw)(uint32_t)) {
  const uint32_t end = t + maxMs;
  while (t < end) {
    t += stepMs;
    f.update(raw(t), t);
    if (f.changed()) return t;
  }
  return 0;
}

static bool alwaysActive(uint32_t) { return true; }

void test_seeded_state_matches_raw() {
  SwitchFilter a(LIMIT, STEP), b(LIMIT, STEP);
  a.begin(true, 0);
  b.begin(false, 0);
  TEST_ASSERT_TRUE(a.active());
  TEST_ASSERT_FALSE(b.active());
  TEST_ASSERT_FALSE(a.changed());
}

void test_clean_closure_1ms_updates() {
  SwitchFilter f(LIMIT, STEP);
  f.begin(false, 0);
  uint32_t t = 0;
  TEST_ASSERT_EQUAL_UINT32(LIMIT, runUntilFlip(f, t, 1, 100, alwaysActive));
  TEST_ASSERT_TRUE(f.active());
  TEST_ASSERT_EQUAL_UINT32(1, f.onsetMs());
  TEST_ASSERT_EQUAL_UINT16(0, f.glitches());
}

void test_clean_closure_4ms_updates() {
  SwitchFilter f(LIMIT, STEP);
  f.begin(false, 0);
  uint32_t t = 0;
  TEST_ASSERT_EQUAL_UINT32(LIMIT, runUntilFlip(f, t, 4, 100, alwaysActive));
}

void test_stalled_loop_sample_cannot_flip_alone() {
  SwitchFilter f(LIMIT, STEP);
  f.begin(false, 0);
  f.update(true, 500);  // one active read after a 500 ms stall
  TEST_ASSERT_FALSE(f.active());
  f.update(false, 501);
  f.update(false, 510);
  TEST_ASSERT_FALSE(f.active());
}

static bool noisyClosure(uint32_t) { return (nextRand() % 100) >= 30; }  // 30 % glitches

void test_closure_with_30pct_glitches_detected_within_bound() {
  // Net gain is 0.7 - 0.3 = 0.4 ms per ms, so about LIMIT / 0.4 = 30 ms expected.
  rng = 1;
  for (int trial = 0; trial < 50; trial++) {
    SwitchFilter f(LIMIT, STEP);
    f.begin(false, 0);
    uint32_t t = 0;
    uint32_t flip = runUntilFlip(f, t, 1, 1000, noisyClosure);
    TEST_ASSERT_NOT_EQUAL(0, flip);
    TEST_ASSERT_LESS_OR_EQUAL_UINT32(8 * LIMIT, flip);  // ~5 std devs above the mean
  }
}

// 1-3 ms spikes separated by at least 10 ms of quiet.
static bool spikes(uint32_t t) { return (t % 20) < (1 + (t / 20) % 3); }

void test_isolated_spikes_never_close() {
  SwitchFilter f(LIMIT, STEP);
  f.begin(false, 0);
  for (uint32_t t = 1; t <= 10000; t++) {
    f.update(spikes(t), t);
    TEST_ASSERT_FALSE(f.active());
  }
  TEST_ASSERT_EQUAL_UINT16(499, f.glitches());  // every completed spike counted
}

void test_release_needs_integrated_inactive_time() {
  SwitchFilter f(LIMIT, STEP);
  f.begin(true, 0);
  uint32_t t = 0;
  for (; t < 5; t++) f.update(false, t + 1);  // 5 ms open: not enough
  TEST_ASSERT_TRUE(f.active());
  for (uint32_t i = 0; i < 20 && f.active(); i++) {
    t++;
    f.update(false, t);
  }
  TEST_ASSERT_FALSE(f.active());
  TEST_ASSERT_EQUAL_UINT32(LIMIT, t);
}

void test_onset_ignores_earlier_isolated_spike() {
  SwitchFilter f(LIMIT, STEP);
  f.begin(false, 0);
  f.update(true, 1);  // spike
  uint32_t t = 1;
  for (; t < 30; t++) f.update(false, t + 1);
  for (uint32_t i = 0; i < 20 && !f.active(); i++) {
    t++;
    f.update(true, t);
  }
  TEST_ASSERT_TRUE(f.active());
  TEST_ASSERT_EQUAL_UINT32(31, f.onsetMs());
  TEST_ASSERT_EQUAL_UINT16(1, f.glitches());
}

void test_millis_wraparound() {
  SwitchFilter f(LIMIT, STEP);
  uint32_t t = 0xFFFFFFFFu - 5;
  f.begin(false, t);
  for (int i = 0; i < LIMIT; i++) {
    t++;
    f.update(true, t);
  }
  TEST_ASSERT_TRUE(f.active());
}

int main() {
  UNITY_BEGIN();
  RUN_TEST(test_seeded_state_matches_raw);
  RUN_TEST(test_clean_closure_1ms_updates);
  RUN_TEST(test_clean_closure_4ms_updates);
  RUN_TEST(test_stalled_loop_sample_cannot_flip_alone);
  RUN_TEST(test_closure_with_30pct_glitches_detected_within_bound);
  RUN_TEST(test_isolated_spikes_never_close);
  RUN_TEST(test_release_needs_integrated_inactive_time);
  RUN_TEST(test_onset_ignores_earlier_isolated_spike);
  RUN_TEST(test_millis_wraparound);
  return UNITY_END();
}
