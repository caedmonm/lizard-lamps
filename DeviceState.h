#pragma once
#include <Arduino.h>
#include <freertos/FreeRTOS.h>
#include <freertos/semphr.h>

#include "ConfigData.h"
#include "TemperatureHistory.h"

struct DeviceStatus {
  bool testActive = false;
  int testLamp = 0;
  int testPercent = 0;
  uint32_t testRemainingSeconds = 0;
  float temperatures[2] = {NAN, NAN};
  bool sensorsHealthy = false;
  bool dimmerOnline = false;
  bool clockValid = false;
  int activeLamp = -1;
  int power = 0;
  bool targetLimited = false;
  int lampPower[2] = {0, 0};
  Control::FloorMode floorMode = Control::FloorMode::Normal;
  float targets[2] = {NAN, NAN};
  unsigned long lastReading = 0;
};
extern DeviceConfig deviceConfig;
extern DeviceStatus deviceStatus;
// RAM only; both web and control tasks access this under stateMutex.
struct LampTest {
  static constexpr uint32_t TimeoutMs = 5UL * 60 * 1000;
  bool active = false;
  int lamp = 0, percent = 0;
  uint32_t adjustedAt = 0;
};
extern LampTest lampTest;
extern SemaphoreHandle_t stateMutex;
extern bool maintenance;
extern bool maintenanceAcknowledged;
extern bool controllerRunning;
extern bool resetRequested;
DeviceConfig configSnapshot();
DeviceStatus statusSnapshot();
bool loadConfig();
bool saveConfig(const DeviceConfig& config);
void startDeviceWeb();

extern TemperatureHistory temperatureHistory;

// Flash maintenance is serialized on the web task. Only the controller owns outputs.
extern bool historyMaintenance;
extern bool historyStorageReady;
extern bool historyStorageHealthy;
bool beginFlashMaintenance(bool forHistory = false);
void endFlashMaintenance();
void startHistoryStorage();
void serviceTemperatureHistory();
bool eraseTemperatureHistory();
