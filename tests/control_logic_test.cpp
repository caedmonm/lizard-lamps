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
  int minute;
  assert(parseTime("00:00", minute) && minute == 0);
  assert(parseTime("23:59", minute) && minute == 1439);
  for (const char* invalid : {"24:00", "12:60", "1:00", "-1:00", "aa:bb", "12:00junk", ""}) assert(!parseTime(invalid, minute));
  float target;
  assert(parseTarget("19.5", target) && target == 19.5f);
  for (const char* invalid : {"", "nan", "inf", "40.1", "4.9", "20junk", "20,5"}) assert(!parseTarget(invalid, target));
  std::cout << "Control calculations, schedule boundaries, midnight wrap, faults and input validation passed.\n";
}
