#pragma once
#include <cmath>
#include <cstddef>
#include <cstdint>

// Bounded RAM storage: 577 x 12-byte samples includes both ends of a 48h window.
// Uptime, rather than wall time, keeps sampling/expiry stable during NTP changes.
class TemperatureHistory {
public:
  static constexpr uint32_t IntervalMs = 5UL * 60 * 1000;
  static constexpr uint32_t RetentionMs = 48UL * 60 * 60 * 1000;
  static constexpr size_t Capacity = RetentionMs / IntervalMs + 1;
  struct Sample { uint32_t uptime; float temperatures[2]; };
  void clear() { head_ = count_ = 0; sampled_ = false; }
  void prune(uint32_t now) {
    while (count_ && uint32_t(now - at(0).uptime) > RetentionMs) {
      head_ = (head_ + 1) % Capacity;
      --count_;
    }
  }
  bool record(uint32_t now, float basking, float ambient, bool fresh) {
    prune(now);
    if (sampled_ && uint32_t(now - lastSample_) < IntervalMs) return false;
    lastSample_ = now;
    sampled_ = true;
    if (count_ == Capacity) { head_ = (head_ + 1) % Capacity; --count_; }
    samples_[(head_ + count_++) % Capacity] = {now, {reading(basking, fresh), reading(ambient, fresh)}};
    return true;
  }
  size_t size() const { return count_; }
  const Sample& at(size_t index) const { return samples_[(head_ + index) % Capacity]; }
private:
  static float reading(float value, bool fresh) {
    // Keep real over-temperature readings visible; disconnected/invalid readings are gaps.
    return fresh && std::isfinite(value) && value >= -55 && value <= 125 ? value : NAN;
  }
  Sample samples_[Capacity] = {};
  size_t head_ = 0, count_ = 0;
  uint32_t lastSample_ = 0;
  bool sampled_ = false;
};
static_assert(sizeof(TemperatureHistory::Sample) == 12, "Check history RAM budget");
static_assert(sizeof(TemperatureHistory) <= 7000, "History must remain below 7 KB");
