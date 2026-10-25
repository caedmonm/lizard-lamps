#pragma once
#include <cstdint>
#include <cstring>
#include "ControlLogic.h"

struct DeviceConfig {
  uint32_t version = 4;
  char ssid[33] = {};
  char wifiPassword[64] = {};
  // Empty means dashboard password protection is disabled.
  char adminPassword[64] = {};
  // [period: 0 day/lamp 1, 1 night/lamp 2][probe: 0 basking, 1 ambient]
  float targets[2][2] = {{19, 19}, {19, 19}};
  int starts[2] = {420, 1140};
  bool targetsSet = false;
  bool scheduleSet = false;
  bool probesSet = false;
  uint8_t probes[2][8] = {};
  bool dimmingEnabled = true;
  float dimmingRange = Control::FadeRange;
  float restartDrop = 2.0f;
  float minimumTemperature = 15.0f;
};
struct LegacyDeviceConfigV3 {
  uint32_t version = 3;
  char ssid[33] = {};
  char wifiPassword[64] = {};
  // Empty means dashboard password protection is disabled.
  char adminPassword[64] = {};
  // [period: 0 day/lamp 1, 1 night/lamp 2][probe: 0 basking, 1 ambient]
  float targets[2][2] = {{19, 19}, {19, 19}};
  int starts[2] = {420, 1140};
  bool targetsSet = false;
  bool scheduleSet = false;
  bool probesSet = false;
  uint8_t probes[2][8] = {};
  bool dimmingEnabled = true;
  float dimmingRange = Control::FadeRange;
  float restartDrop = 2.0f;
};
struct LegacyDeviceConfigV2 {
  uint32_t version = 2;
  char ssid[33] = {};
  char wifiPassword[64] = {};
  // Empty means dashboard password protection is disabled.
  char adminPassword[64] = {};
  // [period: 0 day/lamp 1, 1 night/lamp 2][probe: 0 basking, 1 ambient]
  float targets[2][2] = {{19, 19}, {19, 19}};
  int starts[2] = {420, 1140};
  bool targetsSet = false;
  bool scheduleSet = false;
  bool probesSet = false;
  uint8_t probes[2][8] = {};
};
// Exact previous NVS layout, retained solely for upgrading existing devices.
struct LegacyDeviceConfigV1 {
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
inline DeviceConfig migrateLegacyConfig(const LegacyDeviceConfigV1& old) {
  DeviceConfig result;
  result.version = old.version == 1 ? 4 : 0;
  memcpy(result.ssid, old.ssid, sizeof(result.ssid));
  memcpy(result.wifiPassword, old.wifiPassword, sizeof(result.wifiPassword));
  memcpy(result.adminPassword, old.adminPassword, sizeof(result.adminPassword));
  memcpy(result.starts, old.starts, sizeof(result.starts));
  memcpy(result.probes, old.probes, sizeof(result.probes));
  for (int period = 0; period < 2; ++period)
    for (int probe = 0; probe < 2; ++probe) result.targets[period][probe] = old.targets[probe];
  result.targetsSet = old.targetsSet && old.scheduleSet;
  result.scheduleSet = result.targetsSet;
  result.probesSet = old.probesSet;
  return result;
}
inline DeviceConfig migrateLegacyConfig(const LegacyDeviceConfigV2& old) {
  DeviceConfig result;
  result.version = old.version == 2 ? 4 : 0;
  memcpy(result.ssid, old.ssid, sizeof(result.ssid));
  memcpy(result.wifiPassword, old.wifiPassword, sizeof(result.wifiPassword));
  memcpy(result.adminPassword, old.adminPassword, sizeof(result.adminPassword));
  memcpy(result.targets, old.targets, sizeof(result.targets));
  memcpy(result.starts, old.starts, sizeof(result.starts));
  memcpy(result.probes, old.probes, sizeof(result.probes));
  result.targetsSet = old.targetsSet;
  result.scheduleSet = old.scheduleSet;
  result.probesSet = old.probesSet;
  return result;
}
inline DeviceConfig migrateLegacyConfig(const LegacyDeviceConfigV3& old) {
  DeviceConfig result;
  result.version = old.version == 3 ? 4 : 0;
  memcpy(result.ssid, old.ssid, sizeof(result.ssid));
  memcpy(result.wifiPassword, old.wifiPassword, sizeof(result.wifiPassword));
  memcpy(result.adminPassword, old.adminPassword, sizeof(result.adminPassword));
  memcpy(result.targets, old.targets, sizeof(result.targets));
  memcpy(result.starts, old.starts, sizeof(result.starts));
  memcpy(result.probes, old.probes, sizeof(result.probes));
  result.targetsSet = old.targetsSet;
  result.scheduleSet = old.scheduleSet;
  result.probesSet = old.probesSet;
  result.dimmingEnabled = old.dimmingEnabled;
  result.dimmingRange = old.dimmingRange;
  result.restartDrop = old.restartDrop;
  return result;
}
inline bool validConfig(const DeviceConfig& config) {
  if (config.version != 4 || !Control::validTarget(config.minimumTemperature) || !Control::validControlRange(config.dimmingRange) ||
      !Control::validControlRange(config.restartDrop) || !memchr(config.ssid, 0, sizeof(config.ssid)) ||
      !memchr(config.wifiPassword, 0, sizeof(config.wifiPassword)) ||
      !memchr(config.adminPassword, 0, sizeof(config.adminPassword)) ||
      (config.adminPassword[0] && strlen(config.adminPassword) < 8) ||
      Control::scheduledLamp(0, config.starts[0], config.starts[1]) < 0) return false;
  for (int period = 0; period < 2; ++period)
    for (int probe = 0; probe < 2; ++probe)
      if (!Control::validTarget(config.targets[period][probe])) return false;
  return true;
}
