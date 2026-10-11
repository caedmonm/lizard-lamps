#include "DeviceState.h"
#include "ControlLogic.h"
#include <Preferences.h>
#include <OneWire.h>
DeviceConfig deviceConfig;
DeviceStatus deviceStatus;
SemaphoreHandle_t stateMutex;
bool maintenance = false;
bool maintenanceAcknowledged = false;
bool controllerRunning = false;
bool resetRequested = false;
DeviceConfig configSnapshot() {
  xSemaphoreTake(stateMutex, portMAX_DELAY);
  DeviceConfig copy = deviceConfig;
  xSemaphoreGive(stateMutex);
  return copy;
}
DeviceStatus statusSnapshot() {
  xSemaphoreTake(stateMutex, portMAX_DELAY);
  DeviceStatus copy = deviceStatus;
  xSemaphoreGive(stateMutex);
  return copy;
}
bool loadConfig() {
  Preferences prefs;
  if (!prefs.begin("lizard", true)) return false;
  DeviceConfig candidate;
  bool found = prefs.getBytesLength("config") == sizeof(candidate) &&
               prefs.getBytes("config", &candidate, sizeof(candidate)) == sizeof(candidate);
  prefs.end();
  if (!found || candidate.version != 1 ||
      !memchr(candidate.ssid, 0, sizeof(candidate.ssid)) ||
      !memchr(candidate.wifiPassword, 0, sizeof(candidate.wifiPassword)) ||
      !memchr(candidate.adminPassword, 0, sizeof(candidate.adminPassword)) ||
      !Control::validTarget(candidate.targets[0]) || !Control::validTarget(candidate.targets[1]) ||
      Control::scheduledLamp(0, candidate.starts[0], candidate.starts[1]) < 0 ||
      (candidate.ssid[0] && strlen(candidate.adminPassword) < 8)) return false;
  if (candidate.probesSet) {
    if (memcmp(candidate.probes[0], candidate.probes[1], 8) == 0) return false;
    for (int i = 0; i < 2; ++i)
      if (candidate.probes[i][0] != 0x28 || OneWire::crc8(candidate.probes[i], 7) != candidate.probes[i][7]) return false;
  }
  deviceConfig = candidate;
  return true;
}
bool saveConfig(const DeviceConfig& candidate) {
  // Hold the outputs off while flash is written; only the control loop owns dimmer hardware.
  xSemaphoreTake(stateMutex, portMAX_DELAY);
  maintenance = true;
  maintenanceAcknowledged = false;
  xSemaphoreGive(stateMutex);
  // Wait for the controller to acknowledge zero output before touching flash.
  unsigned long started = millis();
  while (controllerRunning) {
    xSemaphoreTake(stateMutex, portMAX_DELAY);
    bool acknowledged = maintenanceAcknowledged;
    xSemaphoreGive(stateMutex);
    if (acknowledged) break;
    if (millis() - started > 1000) {
      xSemaphoreTake(stateMutex, portMAX_DELAY);
      maintenance = false;
      xSemaphoreGive(stateMutex);
      return false;
    }
    delay(2);
  }
  Preferences prefs;
  bool saved = prefs.begin("lizard", false);
  if (saved) {
    saved = prefs.putBytes("config", &candidate, sizeof(candidate)) == sizeof(candidate);
    prefs.end();
  }
  xSemaphoreTake(stateMutex, portMAX_DELAY);
  if (saved) deviceConfig = candidate;
  maintenance = false;
  xSemaphoreGive(stateMutex);
  return saved;
}
