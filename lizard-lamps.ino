#include <Arduino.h>
#include <OneWire.h>
#include <DallasTemperature.h>
#include <MycilaDimmers.h>
#include <MycilaPulseAnalyzer.h>
#include <time.h>
#include "DeviceState.h"
#include "ControlLogic.h"

constexpr int OneWirePin = 4;
constexpr gpio_num_t ZeroCrossPin = GPIO_NUM_23;
constexpr gpio_num_t DimmerPins[] = {GPIO_NUM_25, GPIO_NUM_26};
OneWire oneWire(OneWirePin);
DallasTemperature sensors(&oneWire);
Mycila::PulseAnalyzer pulseAnalyzer;
Mycila::ThyristorDimmer lamp1, lamp2;
bool dimmerReady = false;
unsigned long conversionStarted = 0;
bool conversionPending = false;
float temperatures[2] = {NAN, NAN};
unsigned long lastReading = 0;
bool sensorReadOK = false;
Control::HeatingController heatingController;
Control::LampFader lampFader;
Control::MinimumTemperatureController minimumController;
uint8_t readingProbes[2][8] = {};

void requestTemperatures() {
  sensors.requestTemperatures();
  conversionStarted = millis();
  conversionPending = true;
}
void applyPower(int day, int night) {
  if (!dimmerReady) return;
  // Write outputs going off first. Only floor recovery intentionally overlaps.
  if (day == 0) lamp1.setDutyCycle(0);
  if (night == 0) lamp2.setDutyCycle(0);
  if (day > 0) lamp1.setDutyCycle(day / 100.0f);
  if (night > 0) lamp2.setDutyCycle(night / 100.0f);
}
void setup() {
  Serial.begin(115200);
  pinMode(DimmerPins[0], OUTPUT);
  pinMode(DimmerPins[1], OUTPUT);
  digitalWrite(DimmerPins[0], LOW);
  digitalWrite(DimmerPins[1], LOW);
  stateMutex = xSemaphoreCreateMutex();
  if (!stateMutex) { Serial.println("State initialization failed; heating disabled"); while (true) delay(1000); }
  loadConfig();
  sensors.begin();
  sensors.setResolution(12);
  sensors.setWaitForConversion(false);
  // Bind discovered addresses once; never silently replace a missing probe.
  if (!deviceConfig.probesSet && sensors.getDeviceCount() == 2) {
    DeviceConfig candidate = deviceConfig;
    candidate.probesSet = sensors.getAddress(candidate.probes[0], 0) &&
                          sensors.getAddress(candidate.probes[1], 1);
    if (candidate.probesSet) {
      candidate.probesSet = candidate.probes[0][0] == 0x28 && candidate.probes[1][0] == 0x28 &&
        memcmp(candidate.probes[0], candidate.probes[1], 8) != 0;
      if (candidate.probesSet) saveConfig(candidate);
    }
  }
  Mycila::Dimmer::setSemiPeriod(10000); // 50Hz mains
  lamp1.setPin(DimmerPins[0]);
  lamp2.setPin(DimmerPins[1]);
  lamp1.enablePowerLUT(true);
  lamp2.enablePowerLUT(true);
  bool first = lamp1.begin();
  bool second = lamp2.begin();
  if (first) lamp1.setDutyCycle(0);
  if (second) lamp2.setDutyCycle(0);
  if (first && second) {
    lamp1.setOnline(true);
    lamp2.setOnline(true);
    pulseAnalyzer.onZeroCross(Mycila::ThyristorDimmer::onZeroCross);
    dimmerReady = pulseAnalyzer.begin(ZeroCrossPin);
  }
  if (!dimmerReady) { lamp1.end(); lamp2.end(); }
  requestTemperatures();
  controllerRunning = true;
  startDeviceWeb();
}
void loop() {
  unsigned long now = millis();
  DeviceConfig config = configSnapshot();
  if (memcmp(readingProbes, config.probes, sizeof(readingProbes)) != 0) {
    memcpy(readingProbes, config.probes, sizeof(readingProbes));
    sensorReadOK = false;
  }
  if (conversionPending && now - conversionStarted >= 750) {
    for (int i = 0; i < 2; ++i)
      temperatures[i] = config.probesSet ? sensors.getTempC(config.probes[i]) : NAN;
    sensorReadOK = config.probesSet && Control::validTemperature(temperatures[0]) &&
                   Control::validTemperature(temperatures[1]);
    lastReading = millis();
    conversionPending = false;
  }
  if (!conversionPending && now - conversionStarted >= 1000) requestTemperatures();
  time_t epoch = time(nullptr);
  bool clockValid = epoch >= 1704067200 && epoch < 4102444800LL;
  tm local = {};
  if (clockValid) localtime_r(&epoch, &local);
  int selected = clockValid && config.scheduleSet ?
    Control::scheduledLamp(local.tm_hour * 60 + local.tm_min, config.starts[0], config.starts[1]) : -1;
  bool healthy = sensorReadOK && (millis() - lastReading <= Control::StaleAfterMs);
  bool online = dimmerReady && pulseAnalyzer.isOnline();
  xSemaphoreTake(stateMutex, portMAX_DELAY);
  bool stopped = maintenance || resetRequested;
  xSemaphoreGive(stateMutex);
  bool hardwareHealthy = healthy && online && !stopped;
  bool enabled = config.targetsSet && config.scheduleSet && selected >= 0 && clockValid && healthy && online && !stopped;
  int power = heatingController.update(enabled, selected, temperatures[0], temperatures[1],
                                       config.targets, config.dimmingEnabled, config.dimmingRange, config.restartDrop);
  Control::FloorDemand floor = minimumController.update(hardwareHealthy, temperatures[0], temperatures[1],
                                                        config.minimumTemperature, enabled && power > 0);
  bool floorActive = hardwareHealthy && floor.mode != Control::FloorMode::Normal;
  Control::LampOutput output = lampFader.update(enabled && !floorActive, selected, power, millis());
  int day = floorActive ? floor.day : (output.lamp == 0 ? output.percent : 0);
  int night = floorActive ? floor.night : (output.lamp == 1 ? output.percent : 0);
  applyPower(day, night);
  power = day > night ? day : night;
  int activeLamp = floorActive ? (floor.mode == Control::FloorMode::Recovery ? 2 : 1) :
                   (output.lamp >= 0 ? output.lamp : selected);
  xSemaphoreTake(stateMutex, portMAX_DELAY);
  deviceStatus.temperatures[0] = temperatures[0];
  deviceStatus.temperatures[1] = temperatures[1];
  deviceStatus.sensorsHealthy = healthy;
  deviceStatus.dimmerOnline = online;
  deviceStatus.clockValid = clockValid;
  deviceStatus.activeLamp = activeLamp;
  deviceStatus.power = power;
  deviceStatus.lampPower[0] = day;
  deviceStatus.lampPower[1] = night;
  deviceStatus.floorMode = floorActive ? floor.mode : Control::FloorMode::Normal;
  for (int probe = 0; probe < 2; ++probe)
    deviceStatus.targets[probe] = floorActive ? config.minimumTemperature : (activeLamp >= 0 ? config.targets[activeLamp][probe] : NAN);
  if (stopped && power == 0) maintenanceAcknowledged = true;
  deviceStatus.lastReading = lastReading;
  xSemaphoreGive(stateMutex);
  delay(2);
}
