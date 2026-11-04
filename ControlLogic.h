#pragma once
#include <cmath>
#include <cstdint>
#include <cstdlib>
#include <cstring>

namespace Control {
constexpr float FadeRange = 3.0f;
constexpr float MaximumTemperature = 45.0f;
constexpr unsigned long StaleAfterMs = 3000;
inline bool validTemperature(float t) {
  return std::isfinite(t) && t >= -20 && t < MaximumTemperature;
}
inline bool validTarget(float t) { return std::isfinite(t) && t >= 5 && t <= 40; }
// A daily interval includes its start and excludes its end, including midnight wrap.
inline int scheduledLamp(int minute, int lamp1Start, int lamp2Start) {
  if (minute < 0 || minute >= 1440 || lamp1Start < 0 || lamp1Start >= 1440 ||
      lamp2Start < 0 || lamp2Start >= 1440 || lamp1Start == lamp2Start) return -1;
  bool first = lamp1Start < lamp2Start ? minute >= lamp1Start && minute < lamp2Start
                                    : minute >= lamp1Start || minute < lamp2Start;
  return first ? 0 : 1;
}
inline bool validFadeSeconds(float value) { return std::isfinite(value) && value >= 0.1f && value <= 300.0f; }
inline bool validProportionalBand(float value) { return std::isfinite(value) && value >= 1 && value <= 10; }
inline bool validIntegralSeconds(float value) { return std::isfinite(value) && value >= 60 && value <= 3600; }
constexpr float TargetOvershoot = 1.0f;
inline bool targetsExceeded(float basking, float ambient, const float targets[2]) {
  return basking >= targets[0] + TargetOvershoot && ambient >= targets[1] + TargetOvershoot;
}
// PI regulation of the largest target deficit: the last probe to reach its
// target controls holding power. Only the control loop owns this state.
class HeatingController {
  float integral = 0;
  int previousPeriod = -1;
  float previousTargets[2] = {NAN, NAN};
  float previousBand = NAN, previousIntegralSeconds = NAN;
  uint32_t lastSample = 0;
  int demand = 0;
  static float limit(float value) { return std::fmax(0.0f, std::fmin(100.0f, value)); }
public:
  void reset() { integral = 0; previousPeriod = -1; demand = 0; }
  // Brief history writes retain learned holding power without integrating time off.
  void pause(uint32_t sampleTime) { lastSample = sampleTime; }
  int update(bool enabled, int selected, float basking, float ambient,
             const float targets[2][2], float band, float integralSeconds,
             uint32_t sampleTime, int appliedPower) {
    if (!enabled || selected < 0 || selected > 1 ||
        !validTemperature(basking) || !validTemperature(ambient) ||
        !validProportionalBand(band) || !validIntegralSeconds(integralSeconds) ||
        appliedPower < 0 || appliedPower > 100 ||
        !validTarget(targets[selected][0]) || !validTarget(targets[selected][1])) {
      reset(); return 0;
    }
    const uint32_t elapsed = sampleTime - lastSample;
    const bool changed = selected != previousPeriod || targets[selected][0] != previousTargets[0] ||
                         targets[selected][1] != previousTargets[1] ||
                         band != previousBand || integralSeconds != previousIntegralSeconds;
    // Never integrate missed/stale samples, or carry heat demand into a new period.
    if (changed || elapsed > StaleAfterMs) integral = 0;
    previousPeriod = selected;
    previousTargets[0] = targets[selected][0]; previousTargets[1] = targets[selected][1];
    previousBand = band; previousIntegralSeconds = integralSeconds;
    lastSample = sampleTime;
    if (targetsExceeded(basking, ambient, previousTargets)) {
      integral = 0; demand = 0; return 0;
    }
    const float error = std::fmax(previousTargets[0] - basking, previousTargets[1] - ambient);
    const float proportional = error * 100.0f / band;
    if (!changed && elapsed > 0 && elapsed <= StaleAfterMs) {
      const float adjustment = proportional * (elapsed / 1000.0f) / integralSeconds;
      // Conditional integration prevents windup at full power, during fade-in,
      // and while the outgoing lamp is still active. Unwinding is always allowed.
      if (adjustment < 0 || (proportional + integral < 100 && appliedPower >= demand - 1))
        integral = limit(integral + adjustment);
    }
    demand = int(std::lround(limit(proportional + integral)));
    return demand;
  }
};
// Time-based output smoothing; faults bypass fading and clear all transition state.
struct LampOutput { int lamp; int percent; };
class LampFader {
  uint32_t durationMs = 1000;
  static constexpr uint32_t SettleMs = 20;
  int lamp = -1, target = 0;
  float current = 0, start = 0;
  uint32_t startedAt = 0, offAt = 0;
  bool settling = false;

  void advance(uint32_t now) {
    uint32_t elapsed = now - startedAt;
    current = durationMs == 0 || elapsed >= durationMs ? target : start + (target - start) * (elapsed / float(durationMs));
  }
  void aim(int demand, uint32_t now) {
    if (target == demand) return; // Repeated loop updates must not restart the fade.
    start = current;
    target = demand;
    startedAt = now;
  }
public:
  LampOutput update(bool enabled, int selected, int demand, uint32_t now, uint32_t fadeMs = 1000) {
    if (!enabled || selected < 0 || selected > 1 || demand < 0 || demand > 100) {
      lamp = -1;
      current = start = 0;
      target = 0;
      settling = false;
      return {-1, 0};
    }
    advance(now);
    if (fadeMs!=durationMs) {
      start=current;startedAt=now;durationMs=fadeMs;
    }
    if (lamp != selected) {
      aim(0, now);
      if (!durationMs) current=0;
      if (current > 0) return {lamp, int(std::lround(current))};
      if (!settling) { settling = true; offAt = now; }
      if (uint32_t(now - offAt) < SettleMs) return {-1, 0};
      lamp = selected;
      settling = false;
    } else {
      settling = false; // A schedule reversal can resume the outgoing lamp.
    }
    aim(demand, now);
    if (!durationMs) current=demand;
    return {lamp, int(std::lround(current))};
  }
};
// Floor protection is independent of wall time and scheduled regulation.
// Only basking drives the floor demand; both probes remain required for safety.
enum class FloorMode { Normal, Recovery, NightHold };
struct FloorDemand { FloorMode mode; int day; int night; };
class MinimumTemperatureController {
  bool recovering = false, holding = false;
  float previousMinimum = NAN;
public:
  FloorDemand update(bool healthy, float basking, float ambient, float minimum,
                     bool scheduledHeating) {
    if (!healthy || !validTemperature(basking) || !validTemperature(ambient) || !validTarget(minimum)) {
      recovering = holding = false;
      return {FloorMode::Normal, 0, 0};
    }
    if (minimum != previousMinimum) recovering = holding = false;
    previousMinimum = minimum;
    if (basking < minimum) recovering = true;
    if (recovering && basking >= minimum) {
      recovering = false;
      holding = true;
    }
    if (recovering) {
      // Day assistance stops as soon as basking rises above the 5 C deficit.
      bool dayAssist = basking <= minimum - 5.0f;
      return {dayAssist ? FloorMode::Recovery : FloorMode::NightHold, dayAssist ? 100 : 0, 100};
    }
    // Night-only thermostat anticipates another dip below the floor by 0.5 C.
    if (basking <= minimum) holding = true;
    else if (basking >= minimum + 0.5f) holding = false;
    if (scheduledHeating) {
      holding = false;
      return {FloorMode::Normal, 0, 0};
    }
    return {holding ? FloorMode::NightHold : FloorMode::Normal, 0, holding ? 100 : 0};
  }
};
inline bool parseFadeSeconds(const char* text, float& value) {
  if (!*text) return false;
  char* end;
  value = std::strtof(text, &end);
  return *end == '\0' && validFadeSeconds(value);
}
inline bool parseProportionalBand(const char* text, float& value) {
  if (!*text) return false;
  char* end;
  value = std::strtof(text, &end);
  return *end == '\0' && validProportionalBand(value);
}
inline bool parseIntegralSeconds(const char* text, float& value) {
  if (!*text) return false;
  char* end;
  value = std::strtof(text, &end);
  return *end == '\0' && validIntegralSeconds(value);
}
inline bool parseTime(const char* text, int& minute) {
  if (std::strlen(text) != 5 || text[2] != ':' || text[0] < '0' || text[0] > '2' ||
      text[1] < '0' || text[1] > '9' || text[3] < '0' || text[3] > '5' ||
      text[4] < '0' || text[4] > '9') return false;
  int hour = (text[0] - '0') * 10 + text[1] - '0';
  if (hour > 23) return false;
  minute = hour * 60 + (text[3] - '0') * 10 + text[4] - '0';
  return true;
}
inline bool parseTarget(const char* text, float& value) {
  if (!*text) return false;
  char* end;
  value = std::strtof(text, &end);
  return *end == '\0' && validTarget(value);
}
}
