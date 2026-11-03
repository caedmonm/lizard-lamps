#pragma once
#include <cmath>
#include <cstddef>
#include <cstdint>

// One entry per UTC minute; includes both ends of a 48-hour window.
class TemperatureHistory {
public:
  static constexpr uint32_t IntervalSeconds = 60;
  static constexpr uint32_t RetentionSeconds = 48UL * 60 * 60;
  static constexpr size_t Capacity = RetentionSeconds / IntervalSeconds + 1;
  static constexpr int16_t Missing = INT16_MIN;
  // Tenths of a degree preserve probe precision and keep the complete log below 58 KB.
  struct Sample {
    uint32_t epoch;
    int16_t temperatures[2];
    int16_t targets[2][2];
    uint8_t power[2];
    // Active-target metadata; zero in records written before browser OTA support.
    uint16_t reserved = 0;
  };
  static int16_t encode(float value) { return std::isfinite(value) ? int16_t(std::lround(value*10)) : Missing; }
  static float decode(int16_t value) { return value == Missing ? NAN : value/10.0f; }
  static float activeTarget(const Sample& sample, int probe) {
    if (sample.reserved & 0x2000)
      return probe == 0 ? (sample.reserved & 0x1fff) / 10.0f : NAN;
    if (sample.reserved & 0x4000) return decode(sample.targets[sample.reserved & 1][probe]);
    return NAN; // Legacy records do not identify their active period.
  }
  static bool validClock(uint32_t now) { return now >= 1704067200UL && now < 4102444800UL; }
  void clear() { head_ = count_ = 0; }
  void prune(uint32_t now) {
    // A backwards clock correction cannot leave future points in the graph.
    while (count_ && at(count_ - 1).epoch > now) --count_;
    while (count_ && now - at(0).epoch > RetentionSeconds) {
      head_ = (head_ + 1) % Capacity;
      --count_;
    }
  }
  bool record(uint32_t now, float basking, float ambient, bool fresh,
              const float targets[2][2], const int power[2], bool configured = true, int activeLamp = -1, float floorTarget = NAN) {
    if (!validClock(now)) return false;
    const uint32_t minute = now - now % IntervalSeconds;
    prune(minute);
    if (count_ && at(count_ - 1).epoch == minute) return false;
    Sample sample = {};
    sample.epoch=minute;
    sample.temperatures[0]=encode(reading(basking,fresh));
    sample.temperatures[1]=encode(reading(ambient,fresh));
    for (int period=0;period<2;++period) {
      sample.power[period]=uint8_t(power[period]<0?0:power[period]>100?100:power[period]);
      for (int probe=0;probe<2;++probe)
        sample.targets[period][probe]=configured?encode(targets[period][probe]):Missing;
    }
    sample.reserved = 0x8000;
    if (std::isfinite(floorTarget)) sample.reserved |= 0x2000 | uint16_t(encode(floorTarget));
    else if (configured && activeLamp >= 0 && activeLamp < 2) sample.reserved |= 0x4000 | uint16_t(activeLamp);
    append(sample);
    return true;
  }
  void append(const Sample& sample) {
    if (count_ == Capacity) { head_ = (head_ + 1) % Capacity; --count_; }
    samples_[(head_ + count_++) % Capacity] = sample;
  }
  size_t size() const { return count_; }
  const Sample& at(size_t index) const { return samples_[(head_ + index) % Capacity]; }
private:
  static float reading(float value, bool fresh) {
    return fresh && std::isfinite(value) && value >= -55 && value <= 125 ? value : NAN;
  }
  Sample samples_[Capacity] = {};
  size_t head_ = 0, count_ = 0;
};
static_assert(sizeof(TemperatureHistory::Sample) == 20, "Check history RAM budget");
static_assert(sizeof(TemperatureHistory) < 58000, "History must remain below 58 KB");
