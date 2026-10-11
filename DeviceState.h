#pragma once
#include <Arduino.h>
#include <freertos/FreeRTOS.h>
#include <freertos/semphr.h>

struct DeviceConfig {
  uint32_t version = 1;
  char ssid[33] = {};
  char wifiPassword[64] = {};
  char adminPassword[64] = {};
  float targets[2] = {19, 19};
  int starts[2] = {420, 1140};
  bool targetsSet = false;
  bool scheduleSet = false;
  bool probesSet = false;
  uint8_t probes[2][8] = {};
};
struct DeviceStatus {
  float temperatures[2] = {NAN, NAN};
  bool sensorsHealthy = false;
  bool dimmerOnline = false;
  bool clockValid = false;
  int activeLamp = -1;
  int power = 0;
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
