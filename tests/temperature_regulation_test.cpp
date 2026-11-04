#include "../ControlLogic.h"
#include <cassert>
#include <iostream>
#include <vector>

struct Rig {
  Control::HeatingController controller;
  float targets[2][2] = {{32, 21}, {20, 18}};
  uint32_t sample = 0;
  int applied = 0;
  int update(float basking, float ambient = 21.8f, bool enabled = true, int period = 0,
             float band = 3, float seconds = 300) {
    sample += 1000;
    return applied = controller.update(enabled, period, basking, ambient, targets, band, seconds, sample, applied);
  }
};

void simulate(float gain, float timeConstant, int delaySeconds) {
  Rig rig;
  Control::LampFader fader;
  Control::ScheduledDemandHold hold;
  std::vector<float> delay(delaySeconds + 1, 0);
  float temperature = 20, maximum = 20;
  int lowest = 100, highest = 0;
  // Delayed first-order enclosure, quantized to the DS18B20's 0.0625 C steps.
  // Include a colder room halfway through to exercise holding-power adaptation.
  for (int second = 0; second < 14400; ++second) {
    float room = second < 7200 ? 20 : 18;
    float measured = std::round(temperature * 16) / 16;
    int demand = rig.controller.update(true, 0, measured, 21.8f, rig.targets, 3, 300,
                                      uint32_t(second * 1000), rig.applied);
    demand = hold.update(!Control::targetsExceeded(measured, 21.8f, rig.targets[0]), 0, demand, uint32_t(second*1000));
    for (int tick = 0; tick < 1000; tick += 20) {
      auto output = fader.update(!Control::targetsExceeded(measured, 21.8f, rig.targets[0]), 0, demand,
                                uint32_t(second * 1000 + tick), 1000);
      rig.applied = output.percent;
    }
    delay[second % delay.size()] = rig.applied / 100.0f;
    float heat = delay[(second + 1) % delay.size()];
    temperature += (room + gain * heat - temperature) / timeConstant;
    maximum = std::fmax(maximum, temperature);
    assert(rig.applied >= 0 && rig.applied <= 100);
    if (second > 13800) {
      assert(std::fabs(temperature - 32) < 0.2f);
      lowest = std::min(lowest, rig.applied); highest = std::max(highest, rig.applied);
    }
  }
  assert(lowest > 0 && highest < 100); // Holds using partial power, not on/off cycles.
  assert(highest - lowest <= 6);
  assert(maximum < 33);
  std::cout << "Thermal model gain=" << gain << " tau=" << timeConstant << " delay=" << delaySeconds
            << ": final " << temperature << " C, peak " << maximum << " C, holding " << lowest << "-" << highest << "%\n";
}

void simulateTwoProbes(float baskingGain, float ambientGain) {
  Rig rig;
  rig.targets[0][1] = 25;
  Control::LampFader fader;
  Control::ScheduledDemandHold hold;
  std::vector<float> delay(21, 0);
  float temperatures[2] = {20, 20};
  const float gains[2] = {baskingGain, ambientGain};
  int beforeCooling = 0;
  for (int second = 0; second < 14400; ++second) {
    const float room = second < 7200 ? 20 : 18;
    float measured[2];
    for (int i = 0; i < 2; ++i) measured[i] = std::round(temperatures[i]*16)/16;
    int demand = rig.controller.update(true, 0, measured[0], measured[1], rig.targets, 3, 300,
                                      uint32_t(second*1000), rig.applied);
    demand = hold.update(!Control::targetsExceeded(measured[0], measured[1], rig.targets[0]), 0, demand, uint32_t(second*1000));
    for (int tick = 0; tick < 1000; tick += 20)
      rig.applied = fader.update(!Control::targetsExceeded(measured[0], measured[1], rig.targets[0]),
                                0, demand, uint32_t(second*1000+tick)).percent;
    delay[second % delay.size()] = rig.applied / 100.0f;
    const float heat = delay[(second + 1) % delay.size()];
    for (int i = 0; i < 2; ++i) {
      temperatures[i] += (room + gains[i]*heat - temperatures[i])/600;
      assert(temperatures[i] < 45);
    }
    if (second == 7199) beforeCooling = rig.applied;
    if (second > 13800) {
      const float lastDeficit = std::fmax(32-temperatures[0], 25-temperatures[1]);
      assert(std::fabs(lastDeficit) < 0.2f);
      assert(rig.applied > 0 && rig.applied < 100);
    }
  }
  assert(rig.applied > beforeCooling);
  const float required = 100*std::fmax(14/baskingGain, 7/ambientGain);
  assert(std::fabs(rig.applied-required) < 2);
  std::cout << "Two-probe model gains=" << baskingGain << "/" << ambientGain
            << ": final " << temperatures[0] << "/" << temperatures[1]
            << " C, power before/after cooling " << beforeCooling << "/" << rig.applied << "%\n";
}

int main() {
  Rig rig;
  assert(rig.update(31.5f) == 17); // The screenshot now requests heat immediately.
  Rig coldAmbient, warmAmbient;
  assert(coldAmbient.update(32, 20.5f) == warmAmbient.update(31.5f, 21));
  assert(coldAmbient.update(32, 5) == 100); // The last target drives demand.
  assert(coldAmbient.update(34, 20.5f) >= 17); // No basking-only cutoff.
  assert(coldAmbient.update(34, 21) == 0);
  assert(coldAmbient.update(31.5f, 21) == 17); // Controlling probe can switch.
  coldAmbient.targets[0][1] = 22;
  assert(coldAmbient.update(32, 21) == 33); // Ambient target changes reset PI.

  assert(rig.update(25) == 100);
  for (int i = 0; i < 3600; ++i) assert(rig.update(25) == 100);
  assert(rig.update(32) == 0); // No integral windup during full-power warm-up.
  for (int i = 0; i < 300; ++i) rig.update(31.5f);
  int atTarget = rig.update(32);
  assert(atTarget >= 16 && atTarget <= 18); // Retains learned power at the target.
  for (int i = 0; i < 60; ++i) assert(rig.update(32) == atTarget);
  assert(rig.update(31.9f) > atTarget); // No restart band.
  assert(rig.update(32.1f) < atTarget);
  assert(rig.update(33, 22) == 0); // Relative ceiling discards learned demand.
  assert(rig.update(32) == 0);
  for (float invalid : {NAN, INFINITY, -127.0f, 45.0f}) {
    rig.update(31.5f);
    assert(rig.update(invalid) == 0);
    assert(rig.update(31.5f, invalid) == 0);
    assert(rig.update(31.5f) == 17);
  }
  assert(rig.update(31, 21, false) == 0);
  assert(rig.update(19.5f, 18, true, 1) == 17);
  rig.targets[1][0] = 19;
  assert(rig.update(19.5f, 18, true, 1) == 0);
  assert(rig.update(31, 21, true, -1) == 0);
  assert(rig.update(31, 21, true, 0, NAN) == 0);
  assert(rig.update(31, 21, true, 0, 3, 0) == 0);

  Rig held;
  int demand = held.update(31);
  // Hundreds of loop iterations with the same conversion cannot add integral.
  for (int i = 0; i < 1000; ++i)
    assert(held.controller.update(true, 0, 31, 21, held.targets, 3, 300, held.sample, demand) == demand);
  // Long fades or a different active lamp must not build unseen heat demand.
  for (int i = 0; i < 1000; ++i) {
    held.applied = 0;
    assert(held.update(31) == demand);
  }
  for (int i = 0; i < 300; ++i) held.update(31);
  assert(held.update(32) > 30);
  held.sample += 10000;
  held.controller.pause(held.sample);
  assert(held.update(32) > 30); // History pause preserves holding power.
  held.sample += 10000;
  assert(held.update(32) == 0); // Unexplained sample gaps discard learned demand.
  Rig rollover;
  rollover.sample = UINT32_MAX - 1500;
  rollover.update(31.5f);
  for (int i = 0; i < 300; ++i) rollover.update(31.5f);
  assert(rollover.update(32) >= 16);
  float value;
  assert(Control::parseProportionalBand("1", value) && Control::parseProportionalBand("10", value));
  assert(Control::parseIntegralSeconds("60", value) && Control::parseIntegralSeconds("3600", value));
  for (const char* bad : {"", "nan", "inf", "0.9", "10.1", "3junk"}) assert(!Control::parseProportionalBand(bad, value));
  for (const char* bad : {"", "nan", "inf", "59", "3601", "300junk"}) assert(!Control::parseIntegralSeconds(bad, value));
  simulate(20, 300, 10);
  simulate(25, 600, 20);
  simulate(30, 900, 30);
  simulateTwoProbes(30, 10); // Ambient reaches its target last.
  simulateTwoProbes(20, 9);  // Basking controls first; ambient takes over as the room cools.
}
