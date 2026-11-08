#include "speed_profile.h"

static const uint32_t ONE = 1024;  // fixed-point 1.0, keeps the math in 32-bit on AVR

// Smoothstep 3s^2 - 2s^3 on 0..ONE (same curve as Space Ships sCurve). Max product ~1.07e9 fits uint32.
static uint32_t sCurve(uint32_t s) { return s * s * (3 * ONE - 2 * s) / (ONE * ONE); }

// from -> to by the fraction num/den (num <= den). diff * den <= 4095 * 65535 fits uint32.
static uint16_t blend(uint16_t from, uint16_t to, uint32_t num, uint32_t den) {
  if (to >= from) return (uint16_t)(from + (uint32_t)(to - from) * num / den);
  return (uint16_t)(from - (uint32_t)(from - to) * num / den);
}

static uint16_t rampUpDuty(uint16_t crawl, uint16_t cruise, uint16_t rampUpMs, uint32_t t) {
  if (t >= rampUpMs) return cruise;
  return blend(crawl, cruise, t, rampUpMs);
}

uint16_t profileDuty(const SpeedProfile &p, uint32_t elapsedMs) {
  const uint16_t cruise = p.cruiseDuty > DUTY_MAX ? DUTY_MAX : p.cruiseDuty;
  const uint16_t crawl = p.crawlDuty > cruise ? cruise : p.crawlDuty;

  if (!p.decel || elapsedMs < p.decelStartMs) return rampUpDuty(crawl, cruise, p.rampUpMs, elapsedMs);

  // Decel from wherever the ramp-up had reached, so a decel that starts early stays continuous.
  const uint32_t t = elapsedMs - p.decelStartMs;
  if (t >= p.decelMs) return crawl;
  const uint16_t from = rampUpDuty(crawl, cruise, p.rampUpMs, p.decelStartMs);
  return blend(from, crawl, sCurve(t * ONE / p.decelMs), ONE);
}

SpeedProfile constantProfile(uint16_t duty) {
  SpeedProfile p = {};
  p.cruiseDuty = duty;
  p.crawlDuty = duty;
  return p;
}
