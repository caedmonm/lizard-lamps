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
inline bool validControlRange(float value) { return std::isfinite(value) && value >= 0.1f && value <= 10.0f; }
inline int power(float basking, float ambient, float targetBasking, float targetAmbient, float range = FadeRange) {
  if (!validTemperature(basking) || !validTemperature(ambient) ||
      !validTarget(targetBasking) || !validTarget(targetAmbient) || !validControlRange(range)) return 0;
  float deficit = std::fmax(targetBasking - basking, targetAmbient - ambient);
  return std::lround(std::fmax(0.0f, std::fmin(1.0f, deficit / range)) * 100);
}
inline int periodPower(int selected, float basking, float ambient, const float targets[2][2]) {
  if (selected < 0 || selected > 1) return 0;
  return power(basking, ambient, targets[selected][0], targets[selected][1]);
}
// Stateful on/off demand: only the control loop owns this state.
class HeatingController {
  bool heating = false;
  int previousPeriod = -1;
  bool previousDimming = true;
  float previousRange = NAN, previousDrop = NAN;
  float previousTargets[2] = {NAN, NAN};
public:
  int update(bool enabled, int selected, float basking, float ambient,
             const float targets[2][2], bool dimming, float range, float drop) {
    if (!enabled || selected < 0 || selected > 1 ||
        !validTemperature(basking) || !validTemperature(ambient) ||
        !validControlRange(range) || !validControlRange(drop) ||
        !validTarget(targets[selected][0]) || !validTarget(targets[selected][1])) {
      heating = false;
      previousPeriod = -1;
      return 0;
    }
    if (selected != previousPeriod || dimming != previousDimming ||
        range != previousRange || drop != previousDrop ||
        targets[selected][0] != previousTargets[0] || targets[selected][1] != previousTargets[1])
      heating = false;
    previousPeriod = selected;
    previousDimming = dimming;
    previousRange = range;
    previousDrop = drop;
    previousTargets[0] = targets[selected][0];
    previousTargets[1] = targets[selected][1];
    if (dimming) {
      heating = false;
      return power(basking, ambient, targets[selected][0], targets[selected][1], range);
    }
    float deficit = std::fmax(targets[selected][0] - basking, targets[selected][1] - ambient);
    if (deficit <= 0) heating = false;
    else if (deficit >= drop) heating = true;
    return heating ? 100 : 0;
  }
};
// Time-based output smoothing; faults bypass fading and clear all transition state.
struct LampOutput { int lamp; int percent; };
class LampFader {
  static constexpr uint32_t DurationMs = 10000;
  static constexpr uint32_t SettleMs = 20;
  int lamp = -1, target = 0;
  float current = 0, start = 0;
  uint32_t startedAt = 0, offAt = 0;
  bool settling = false;

  void advance(uint32_t now) {
    uint32_t elapsed = now - startedAt;
    current = elapsed >= DurationMs ? target : start + (target - start) * (elapsed / float(DurationMs));
  }
  void aim(int demand, uint32_t now) {
    if (target == demand) return; // Repeated loop updates must not restart the fade.
    start = current;
    target = demand;
    startedAt = now;
  }
public:
  LampOutput update(bool enabled, int selected, int demand, uint32_t now) {
    if (!enabled || selected < 0 || selected > 1 || demand < 0 || demand > 100) {
      lamp = -1;
      current = start = 0;
      target = 0;
      settling = false;
      return {-1, 0};
    }
    advance(now);
    if (lamp != selected) {
      aim(0, now);
      if (current > 0) return {lamp, int(std::lround(current))};
      if (!settling) { settling = true; offAt = now; }
      if (uint32_t(now - offAt) < SettleMs) return {-1, 0};
      lamp = selected;
      settling = false;
    } else {
      settling = false; // A schedule reversal can resume the outgoing lamp.
    }
    aim(demand, now);
    return {lamp, int(std::lround(current))};
  }
};
// Floor protection is independent of wall time and scheduled hysteresis.
// Both probes must recover. Hardware health and maintenance always take priority.
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
    const float coldest = std::fmin(basking, ambient);
    if (coldest < minimum) recovering = true;
    if (recovering && coldest >= minimum) {
      recovering = false;
      holding = true;
    }
    if (recovering) return {FloorMode::Recovery, 100, 100};
    // Night-only thermostat anticipates another dip below the floor by 0.5 C.
    if (coldest <= minimum) holding = true;
    else if (coldest >= minimum + 0.5f) holding = false;
    if (scheduledHeating) {
      holding = false;
      return {FloorMode::Normal, 0, 0};
    }
    return {holding ? FloorMode::NightHold : FloorMode::Normal, 0, holding ? 100 : 0};
  }
};
inline bool parseControlRange(const char* text, float& value) {
  if (!*text) return false;
  char* end;
  value = std::strtof(text, &end);
  return *end == '\0' && validControlRange(value);
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
