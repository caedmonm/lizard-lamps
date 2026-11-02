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
  assert(!validTarget(4.9f) && !validTarget(40.1f));
  assert(validTarget(5) && validTarget(40));
  float dayNightTargets[2][2]={{30,25},{20,18}};
  HeatingController controller;
  auto demand=[&](float basking,float ambient,bool enabled=true,int period=0,float drop=2) {
    return controller.update(enabled,period,basking,ambient,dayNightTargets,drop);
  };
  assert(demand(29.9f,25)==100); // Even a tiny startup deficit reaches the target.
  assert(demand(30,24.9f)==100); // Both probes must reach target.
  assert(demand(30,25)==0);
  assert(demand(29.9f,24.9f)==0); // Restart hysteresis prevents chatter.
  assert(demand(28.01f,25)==0);
  assert(demand(28,25)==100); // Exact restart boundary.
  assert(demand(29.99f,25)==100); // Full demand right up to target.
  assert(demand(31,26)==0);
  assert(demand(30,23)==100); // Ambient can restart heating.
  for (float bad : {NAN,INFINITY,-127.0f,45.0f,85.0f}) {
    assert(demand(bad,25)==0);assert(demand(29.9f,25)==100);
    assert(demand(30,bad)==0);assert(demand(29.9f,25)==100);
  }
  assert(demand(29,25,false)==0);assert(demand(29,25)==100);
  assert(demand(19.9f,18,true,1)==100); // New period reaches its own targets.
  assert(demand(20,18,true,1)==0);
  assert(demand(19,18,true,1,1)==100); // Configured restart drop.
  dayNightTargets[1][0]=19;
  assert(demand(19,18,true,1,1)==0);
  dayNightTargets[1][0]=19.1f;
  assert(demand(19,18,true,1,1)==100); // New target takes effect immediately.
  assert(demand(29,25,true,-1)==0);
  assert(demand(29,25,true,0,NAN)==0);
  assert(demand(29,25,true,0,0)==0);
  assert(validFadeSeconds(0.1f)&&validFadeSeconds(300));
  for (float bad : {NAN,INFINITY,0.0f,300.1f}) assert(!validFadeSeconds(bad));
  // Configurable smoothing: default one second, half-second midpoint, immediate fault off.
  LampFader defaults;
  defaults.update(true,0,100,0);defaults.update(true,0,100,20);
  assert(defaults.update(true,0,100,520).percent==50);
  assert(defaults.update(true,0,100,1020).percent==100);
  defaults.update(true,0,0,1020);
  assert(defaults.update(true,0,0,1520).percent==50);
  assert(defaults.update(false,0,0,1521).percent==0);
  LampFader instant;
  instant.update(true,0,100,0,0);
  assert(instant.update(true,0,100,20,0).percent==100);
  assert(instant.update(true,0,0,21,0).percent==0);
  instant.update(true,0,100,22,0);
  assert(instant.update(true,1,100,23,0).percent==0); // Switching off has no residual ramp.
  assert(instant.update(true,1,100,43,0).percent==100);
  // Existing ten-second cases exercise a custom duration and repeated-loop deadlines.
  LampFader fader;
  auto fade = [&](int selected, int demand, uint32_t now, bool enabled = true) {
    return fader.update(enabled, selected, demand, now, 10000);
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
  reverse.update(true, 1, 100, 0, 10000);
  reverse.update(true, 1, 100, 20, 10000);
  assert(reverse.update(true, 1, 100, 10020, 10000).percent == 100);
  reverse.update(true, 0, 60, 10020, 10000);
  auto reverseOutgoing = reverse.update(true, 0, 60, 15020, 10000);
  assert(reverseOutgoing.lamp == 1 && reverseOutgoing.percent == 50);
  assert(reverse.update(true, 0, 60, 20020, 10000).lamp == -1);
  reverse.update(true, 0, 60, 20040, 10000);
  auto reverseIncoming = reverse.update(true, 0, 60, 30040, 10000);
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
  rollover.update(true, 0, 100, UINT32_MAX - 10, 10000);
  assert(rollover.update(true, 0, 100, 9, 10000).percent == 0);
  assert(rollover.update(true, 0, 100, 5009, 10000).percent == 50);
  assert(rollover.update(true, 0, 100, 10009, 10000).percent == 100);
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
