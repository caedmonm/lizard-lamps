#pragma once
#include <Arduino.h>
#include <freertos/FreeRTOS.h>
#include <freertos/semphr.h>

#include "ConfigData.h"

struct DeviceStatus {
  float temperatures[2] = {NAN, NAN};
  bool sensorsHealthy = false;
  bool dimmerOnline = false;
  bool clockValid = false;
  int activeLamp = -1;
  int power = 0;
  float targets[2] = {NAN, NAN};
  unsigned long lastReading = 0;
};
extern DeviceConfig deviceConfig;
extern DeviceStatus deviceStatus;
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
