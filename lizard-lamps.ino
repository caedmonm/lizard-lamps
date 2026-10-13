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
uint8_t readingProbes[2][8] = {};

void requestTemperatures() {
  sensors.requestTemperatures();
  conversionStarted = millis();
  conversionPending = true;
}
int applyPower(int selected, int percent) {
  if (!dimmerReady) return 0;
  // Always write the inactive output first, including reverse transitions.
  if (selected == 0) {
    lamp2.setDutyCycle(0);
    lamp1.setDutyCycle(percent / 100.0f);
  } else {
    lamp1.setDutyCycle(0);
    lamp2.setDutyCycle(selected == 1 ? percent / 100.0f : 0);
  }
  return selected >= 0 ? percent : 0;
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
  bool enabled = config.targetsSet && config.scheduleSet && selected >= 0 && clockValid && healthy && online && !stopped;
  int power = heatingController.update(enabled, selected, temperatures[0], temperatures[1],
                                       config.targets, config.dimmingEnabled, config.dimmingRange, config.restartDrop);
  Control::LampOutput output = lampFader.update(enabled, selected, power, millis());
  power = applyPower(output.lamp, output.percent);
  int activeLamp = output.lamp >= 0 ? output.lamp : selected;
  xSemaphoreTake(stateMutex, portMAX_DELAY);
  deviceStatus.temperatures[0] = temperatures[0];
  deviceStatus.temperatures[1] = temperatures[1];
  deviceStatus.sensorsHealthy = healthy;
  deviceStatus.dimmerOnline = online;
  deviceStatus.clockValid = clockValid;
  deviceStatus.activeLamp = activeLamp;
  deviceStatus.power = power;
  for (int probe = 0; probe < 2; ++probe)
    deviceStatus.targets[probe] = activeLamp >= 0 ? config.targets[activeLamp][probe] : NAN;
  if (stopped && power == 0) maintenanceAcknowledged = true;
  deviceStatus.lastReading = lastReading;
  xSemaphoreGive(stateMutex);
  delay(2);
}
