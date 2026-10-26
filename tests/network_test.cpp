#include "../DeviceWeb.cpp"
#include <cassert>
DeviceConfig deviceConfig;
DeviceStatus deviceStatus;
SemaphoreHandle_t stateMutex = 1;
bool maintenance=false, maintenanceAcknowledged=false, controllerRunning=false, resetRequested=false;
DeviceConfig configSnapshot(){return deviceConfig;}
DeviceStatus statusSnapshot(){return deviceStatus;}
bool saveConfig(const DeviceConfig& c){deviceConfig=c;return true;}
bool loadConfig(){return true;}
int main() {
  WiFi.connected = false;
  strcpy(deviceConfig.ssid, "Home");
  connectNetwork();
  assert(WiFi.beginCalls == 1);
  testMillis += 30001; networkTick();
  assert(hotspot && WiFi.beginCalls == 1);
  const auto offlineSince = disconnectedSince;
  testMillis = 60099; networkTick(); assert(WiFi.beginCalls == 1);
  testMillis = 60100; networkTick(); assert(WiFi.beginCalls == 2);
  assert(hotspot && disconnectedSince == offlineSince);
  assert(!WiFi.radioDisabled && !WiFi.credentialsErased);
  for (int retry = 3; retry <= 6; ++retry) {
    testMillis += 59999; networkTick(); assert(WiFi.beginCalls == retry - 1);
    ++testMillis; networkTick(); assert(WiFi.beginCalls == retry);
  }
  WiFi.connected = true; networkTick(); assert(MDNS.started);
  testMillis += 120001; networkTick();
  assert(!hotspot && WiFi.beginCalls == 6); // Healthy Wi-Fi is never restarted.
  WiFi.connected = false; networkTick(); assert(!MDNS.started);
  testMillis += 30001; networkTick(); assert(hotspot && WiFi.beginCalls == 6);
  testMillis += 29999; networkTick(); assert(WiFi.beginCalls == 7);
  // New credentials retry immediately, without waiting for the periodic timer.
  strcpy(deviceConfig.ssid, "New network"); reconnectRequested = true;
  ++testMillis; networkTick(); assert(WiFi.beginCalls == 8 && WiFi.lastSSID == "New network");
  assert(hotspot);
  // Timer remains correct across ESP32 millis() rollover.
  testMillis = UINT32_MAX - 30000; connectNetwork();
  const int attempts = WiFi.beginCalls;
  testMillis += 59999; networkTick(); assert(WiFi.beginCalls == attempts);
  ++testMillis; networkTick(); assert(WiFi.beginCalls == attempts + 1);
  // First-time setup without saved credentials does not make station attempts.
  deviceConfig.ssid[0] = 0; connectNetwork();
  const int provisionAttempts = WiFi.beginCalls;
  testMillis += 180000; networkTick();
  assert(WiFi.beginCalls == provisionAttempts && hotspot);
  std::cout << "Network retries, exact timing, repeated outages, recovery hotspot, credential changes and rollover passed.\n";
}
