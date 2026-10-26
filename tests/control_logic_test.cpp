#include "../ControlLogic.h"
#include <cassert>
#include <limits>
#include <iostream>
int main() {
  using namespace Control;
  assert(scheduledLamp(0, 420, 1140) == 1);
  assert(scheduledLamp(419, 420, 1140) == 1);
  assert(scheduledLamp(420, 420, 1140) == 0);
  assert(scheduledLamp(1139, 420, 1140) == 0);
  assert(scheduledLamp(1140, 420, 1140) == 1);
  assert(scheduledLamp(1439, 420, 1140) == 1);
  assert(scheduledLamp(0, 1140, 420) == 0);
  assert(scheduledLamp(420, 1140, 420) == 1);
  assert(scheduledLamp(1140, 1140, 420) == 0);
  assert(scheduledLamp(10, 5, 5) == -1);
  assert(scheduledLamp(-1, 420, 1140) == -1);
  assert(scheduledLamp(1440, 420, 1140) == -1);
  // Every valid minute belongs to exactly one lamp, with the intended duration.
  for (int first : {0, 1, 420, 1140, 1439}) {
    for (int second : {0, 1, 420, 1140, 1439}) {
      if (first == second) continue;
      int firstMinutes = 0;
      for (int minute = 0; minute < 1440; ++minute) {
        int lamp = scheduledLamp(minute, first, second);
        assert(lamp == 0 || lamp == 1);
        firstMinutes += lamp == 0;
      }
      assert(firstMinutes == (second - first + 1440) % 1440);
    }
  }
  assert(power(19, 19, 19, 19) == 0);
  assert(power(20, 20, 19, 19) == 0);
  assert(power(18.5, 19, 19, 19) == 17);
  assert(power(19, 17, 19, 19) == 67);
  assert(power(10, 19, 19, 19) == 100);
  assert(power(19, 10, 19, 19) == 100);
  assert(power(-127, 19, 19, 19) == 0);
  assert(power(std::numeric_limits<float>::quiet_NaN(), 19, 19, 19) == 0);
  assert(power(45, 10, 19, 19) == 0);
  assert(power(85, 10, 19, 19) == 0);
  assert(power(19, 10, std::numeric_limits<float>::infinity(), 19) == 0);
  assert(!validTarget(4.9f) && !validTarget(40.1f));
  assert(validTarget(5) && validTarget(40));
  float dayNightTargets[2][2] = {{30, 25}, {20, 18}};
  assert(periodPower(scheduledLamp(419, 420, 1140), 28, 24, dayNightTargets) == 0);
  assert(periodPower(scheduledLamp(420, 420, 1140), 28, 24, dayNightTargets) == 67);
  assert(periodPower(scheduledLamp(1140, 420, 1140), 28, 24, dayNightTargets) == 0);
  assert(periodPower(1, 18, 17, dayNightTargets) == 67);
  assert(periodPower(-1, 18, 17, dayNightTargets) == 0);
  assert(power(28, 25, 30, 25, 4) == 50);
  assert(power(28, 25, 30, 25, 2) == 100);
  assert(power(28, 25, 30, 25, 0) == 0);
  HeatingController controller;
  auto demand = [&](float basking, float ambient, bool enabled = true, int period = 0,
                    bool dimming = false, float range = 3, float drop = 2) {
    return controller.update(enabled, period, basking, ambient, dayNightTargets, dimming, range, drop);
  };
  // Startup waits for the restart threshold, then retains full output through the band.
  assert(demand(29, 25) == 0);
  assert(demand(28, 25) == 100);
  assert(demand(29.9f, 25) == 100);
  assert(demand(30, 24) == 100); // Ambient still requests heat.
  assert(demand(30, 25) == 0);
  assert(demand(29, 24) == 0);
  assert(demand(30, 23) == 100); // Either probe can restart heating.
  assert(demand(31, 26) == 0);
  assert(demand(28, 25) == 100);
  assert(demand(45, 25) == 0);
  assert(demand(29, 25) == 0); // Fault clears the previous on state.
  assert(demand(28, 25) == 100);
  assert(demand(29, 25, false) == 0);
  assert(demand(29, 25) == 0);
  assert(demand(28, 25) == 100);
  assert(demand(19, 18, true, 1) == 0); // New period starts from off.
  assert(demand(18, 18, true, 1) == 100);
  assert(demand(19.5f, 18, true, 1) == 100);
  assert(demand(19.5f, 18, true, 1, false, 3, 1) == 0); // Settings clear the latch.
  assert(demand(19, 18, true, 1, false, 3, 1) == 100);
  dayNightTargets[1][0] = 19.5f;
  assert(demand(19, 18, true, 1, false, 3, 1) == 0); // Target edits clear it too.
  assert(demand(28, 25, true, 0, true, 4) == 50);
  assert(demand(29, 25) == 0); // Returning to on/off does not inherit dimming demand.
  assert(demand(28, 25) == 100);
  assert(demand(28, NAN) == 0);
  assert(demand(28, 25, true, -1) == 0);
  assert(demand(28, 25, true, 0, false, NAN) == 0);
  LampFader fader;
  auto fade = [&](int selected, int demand, uint32_t now, bool enabled = true) {
    return fader.update(enabled, selected, demand, now);
  };
  assert(fade(0, 100, 0).percent == 0);
  assert(fade(0, 100, 19).lamp == -1);
  assert(fade(0, 100, 20).percent == 0);
  assert(fade(0, 100, 5020).percent == 50);
  assert(fade(0, 100, 10020).percent == 100);
  assert(fade(0, 0, 10020).percent == 100);
  assert(fade(0, 0, 15020).percent == 50);
  assert(fade(0, 0, 20020).percent == 0);
  // Partial demand also takes ten seconds; frequent calls retain the deadline.
  fade(0, 40, 20020);
  for (uint32_t now = 20020; now < 30020; now += 2) fade(0, 40, now);
  assert(fade(0, 40, 30020).percent == 40);
  // Day/night changes fade the old output out, settle, then fade the new one in.
  assert(fade(1, 80, 30020).lamp == 0);
  auto outgoing = fade(1, 80, 35020);
  assert(outgoing.lamp == 0 && outgoing.percent == 20);
  assert(fade(1, 80, 40020).lamp == -1);
  assert(fade(1, 80, 40039).lamp == -1);
  auto incoming = fade(1, 80, 40040);
  assert(incoming.lamp == 1 && incoming.percent == 0);
  assert(fade(1, 80, 45040).percent == 40);
  assert(fade(1, 80, 50040).percent == 80);
  // Reverse switch also keeps the outgoing channel exclusive until it is off.
  LampFader reverse;
  reverse.update(true, 1, 100, 0);
  reverse.update(true, 1, 100, 20);
  assert(reverse.update(true, 1, 100, 10020).percent == 100);
  reverse.update(true, 0, 60, 10020);
  auto reverseOutgoing = reverse.update(true, 0, 60, 15020);
  assert(reverseOutgoing.lamp == 1 && reverseOutgoing.percent == 50);
  assert(reverse.update(true, 0, 60, 20020).lamp == -1);
  reverse.update(true, 0, 60, 20040);
  auto reverseIncoming = reverse.update(true, 0, 60, 30040);
  assert(reverseIncoming.lamp == 0 && reverseIncoming.percent == 60);
  // A reversal resumes smoothly from the current output without switching lamps.
  fade(0, 100, 50040);
  assert(fade(0, 100, 55040).percent == 40);
  assert(fade(1, 80, 55040).percent == 40);
  assert(fade(1, 80, 60040).percent == 60);
  // Faults, maintenance and invalid selection stop immediately and clear the fade.
  assert(fade(1, 80, 60040, false).percent == 0);
  assert(fade(1, 80, 60041).percent == 0);
  assert(fade(-1, 80, 60042).lamp == -1);
  assert(fade(1, 101, 60043).percent == 0);
  // Millisecond rollover during both the settling gap and a ramp.
  LampFader rollover;
  rollover.update(true, 0, 100, UINT32_MAX - 10);
  assert(rollover.update(true, 0, 100, 9).percent == 0);
  assert(rollover.update(true, 0, 100, 5009).percent == 50);
  assert(rollover.update(true, 0, 100, 10009).percent == 100);
  MinimumTemperatureController floor;
  auto protect = [&](float basking, float ambient, bool hardware = true, bool schedule = false) {
    return floor.update(hardware, basking, ambient, 15, schedule);
  };
  // Cold boot without Wi-Fi/clock: exactly 5 C below minimum enables both lamps.
  auto recovery = protect(10, 12);
  assert(recovery.mode == FloorMode::Recovery && recovery.day == 100 && recovery.night == 100);
  assert(protect(9.9f, 20).day == 100);
  auto nightOnly = protect(10.1f, 12, true, true);
  assert(nightOnly.mode == FloorMode::NightHold && nightOnly.day == 0 && nightOnly.night == 100);
  assert(protect(14.9f, 10, true, true).day == 0); // Night alone overrides scheduled day demand.
  auto hold = protect(15, 10);
  assert(hold.mode == FloorMode::NightHold && hold.day == 0 && hold.night == 100);
  assert(protect(15.4f, 5).night == 100);
  assert(protect(15.5f, 5).mode == FloorMode::Normal); // Only basking releases holding.
  assert(protect(15.1f, 5).night == 0);
  assert(protect(15, 16).night == 100);
  assert(protect(14.9f, 16).day == 0);
  assert(protect(10, 16).day == 100); // A fresh severe dip re-enables day assistance.
  assert(protect(16, 5, true, true).mode == FloorMode::Normal); // Cold ambient cannot trigger the floor.
  assert(protect(16, 5).night == 0);
  // Invalid or overheated ambient still stops heating, despite not setting demand.
  for (float bad : {NAN, INFINITY, -127.0f, 45.0f, 85.0f}) {
    auto stopped = protect(10, bad);
    assert(stopped.day == 0 && stopped.night == 0 && stopped.mode == FloorMode::Normal);
    assert(protect(10, 12).day == 100); // Healthy readings recover without reboot.
    stopped = protect(bad, 12);
    assert(stopped.day == 0 && stopped.night == 0);
  }
  auto stopped = protect(10, 12, false); // Stale, ZC loss, maintenance/reset.
  assert(stopped.day == 0 && stopped.night == 0);
  assert(protect(10, 12).night == 100);
  assert(floor.update(true, 10, 12, NAN, false).day == 0);
  assert(floor.update(true, 10, 12, 41, false).night == 0);
  auto changed = floor.update(true, 18, 18, 19, false);
  assert(changed.day == 0 && changed.night == 100); // Setting changes apply immediately.
  assert(floor.update(true, 14, 18, 19, false).day == 100);
  float controlRange;
  assert(parseControlRange("0.1", controlRange) && controlRange == 0.1f);
  assert(parseControlRange("10", controlRange) && controlRange == 10);
  for (const char* invalid : {"", "nan", "inf", "0", "-2", "10.1", "2junk"})
    assert(!parseControlRange(invalid, controlRange));
  int minute;
  assert(parseTime("00:00", minute) && minute == 0);
  assert(parseTime("23:59", minute) && minute == 1439);
  for (const char* invalid : {"24:00", "12:60", "1:00", "-1:00", "aa:bb", "12:00junk", ""}) assert(!parseTime(invalid, minute));
  float target;
  assert(parseTarget("19.5", target) && target == 19.5f);
  for (const char* invalid : {"", "nan", "inf", "40.1", "4.9", "20junk", "20,5"}) assert(!parseTarget(invalid, target));
  std::cout << "Control calculations, schedule boundaries, midnight wrap, faults and input validation passed.\n";
}
