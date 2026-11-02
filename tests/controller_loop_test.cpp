// Execute the real firmware loop, including conversions, fades, floor routing
// and output-off paths. The dimmers, sensors, clock and storage are mocked.
#include <time.h>
#include <cassert>
time_t testEpoch = 1704110400; // 2024-01-01 12:00 UTC
time_t testTime(time_t*) { return testEpoch; }
#define time testTime
#include "../lizard-lamps.ino"
#undef time
#include "../DeviceState.cpp"
void startDeviceWeb(){}
void startHistoryStorage(){}
void runFor(uint32_t ms) {
  const uint32_t start = testMillis;
  while (uint32_t(testMillis-start) < ms) {
    loop();
    if (deviceStatus.floorMode == Control::FloorMode::Normal)
      assert(lamp1.duty == 0 || lamp2.duty == 0);
  }
}
void read(float basking, float ambient = 21.8f) {
  sensors.readings[0] = basking; sensors.readings[1] = ambient;
  runFor(2100); // A fresh conversion plus the default fade.
}
int main() {
  setenv("TZ", "UTC0", 1); tzset();
  setup();
  assert(lamp1.duty == 0 && lamp2.duty == 0);
  deviceConfig.probesSet = deviceConfig.targetsSet = deviceConfig.scheduleSet = true;
  deviceConfig.probes[0][1]=0; deviceConfig.probes[1][1]=1;
  deviceConfig.targets[0][0]=32; deviceConfig.targets[0][1]=21;
  deviceConfig.targets[1][0]=32; deviceConfig.targets[1][1]=21;
  read(31.5f);
  assert(deviceStatus.power >= 16 && deviceStatus.power <= 18 && lamp2.duty == 0);
  runFor(300000);
  read(32);
  int holding = deviceStatus.power;
  assert(holding >= 16 && holding <= 18 && lamp1.duty > 0);
  maintenance = historyMaintenance = true;
  runFor(5000);
  assert(maintenanceAcknowledged && lamp1.duty == 0 && lamp2.duty == 0);
  maintenance = historyMaintenance = false;
  runFor(2000);
  assert(deviceStatus.power == holding); // Log writes retain learned holding power.
  maintenance = historyMaintenance = true;
  read(32, 45); // A fault during a history pause must still discard learned power.
  read(32);
  maintenance = historyMaintenance = false;
  runFor(2000);
  assert(deviceStatus.power == 0);
  read(33, 22);
  assert(deviceStatus.targetLimited && deviceStatus.power == 0 && lamp1.duty == 0);
  read(32);
  assert(!deviceStatus.targetLimited && deviceStatus.power == 0);
  read(25, 5);
  assert(deviceStatus.power == 100);
  read(34, 20.5f);
  assert(deviceStatus.power > 0 && !deviceStatus.targetLimited); // Ambient is last; keep regulating.
  read(32, 21.8f);
  assert(deviceStatus.power < 2);
  for (int fault = 0; fault < 4; ++fault) {
    read(25);
    assert(lamp1.duty == 1);
    if (fault == 0) pulseAnalyzer.online=false;
    if (fault == 1) maintenance=true;
    if (fault == 2) resetRequested=true;
    if (fault == 3) {lastReading=testMillis-4000;conversionStarted=testMillis;conversionPending=true;}
    loop();
    assert(lamp1.duty == 0 && lamp2.duty == 0);
    pulseAnalyzer.online=true; maintenance=resetRequested=false;
  }
  read(25, 45); assert(deviceStatus.power == 0 && !deviceStatus.sensorsHealthy);
  read(NAN); assert(deviceStatus.power == 0);
  read(25);
  testEpoch += 7*3600; // Day -> night while heating.
  runFor(2); assert(lamp1.duty > 0 && lamp2.duty == 0);
  runFor(2300); assert(lamp1.duty == 0 && lamp2.duty == 1);
  assert(deviceStatus.activeLamp == 1);
  read(14);
  assert(deviceStatus.floorMode == Control::FloorMode::NightHold && lamp1.duty == 0 && lamp2.duty == 1);
  read(10);
  assert(deviceStatus.floorMode == Control::FloorMode::Recovery && lamp1.duty == 1 && lamp2.duty == 1);
  read(32); assert(deviceStatus.power == 0); // Floor did not wind up PI demand.
  testEpoch = 0;
  read(25); assert(deviceStatus.power == 0);
  read(14); assert(lamp2.duty == 1); // Floor still works without a clock.
  read(14, -127); assert(deviceStatus.power == 0);
  std::cout << "Actual control loop: regulation, history pause, transitions, floor, ceiling, clock loss and fault shutdown passed with mocked hardware.\n";
}
