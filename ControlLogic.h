#pragma once
#include <cmath>
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
inline int power(float basking, float ambient, float targetBasking, float targetAmbient) {
  if (!validTemperature(basking) || !validTemperature(ambient) ||
      !validTarget(targetBasking) || !validTarget(targetAmbient)) return 0;
  float deficit = std::fmax(targetBasking - basking, targetAmbient - ambient);
  return std::lround(std::fmax(0.0f, std::fmin(1.0f, deficit / FadeRange)) * 100);
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
