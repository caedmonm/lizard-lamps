#include "DeviceState.h"
#include "HistoryStorage.h"
#include <esp_partition.h>
#include <time.h>
#include <algorithm>

bool historyStorageReady = false;
bool historyStorageHealthy = false;
namespace {
HistoryStorage store;
uint8_t historyProbes[2][8] = {};
bool loaded = false;
uint32_t savedThrough = 0, lastAttempt = 0;
bool attempted = false;

bool blankPartition() {
  const esp_partition_t* partition = esp_partition_find_first(ESP_PARTITION_TYPE_DATA,
                                                            ESP_PARTITION_SUBTYPE_DATA_SPIFFS, "spiffs");
  if (!partition || partition->size < 1024UL*1024) return false;
  uint8_t bytes[256];
  for (size_t offset=0; offset<partition->size; offset+=sizeof(bytes)) {
    const size_t length=std::min<size_t>(sizeof(bytes),partition->size-offset);
    if (esp_partition_read(partition,offset,bytes,length) != ESP_OK) return false;
    for (size_t i=0;i<length;++i) if (bytes[i]!=0xff) return false;
    if (offset % 4096 == 0) delay(1);
  }
  return true;
}
}
void startHistoryStorage() {
  // Called before dimmer initialization. Never format a nonblank/unmountable partition.
  if (!LittleFS.begin(false)) {
    if (!blankPartition() || !LittleFS.format() || !LittleFS.begin(false)) {
      Serial.println("Temperature history flash unavailable; existing filesystem was not erased");
      return;
    }
  }
  historyStorageReady = LittleFS.totalBytes() >= 1024UL*1024;
  historyStorageHealthy = historyStorageReady;
  DeviceConfig config = configSnapshot();
  memcpy(historyProbes,config.probes,sizeof(historyProbes));
  Serial.printf("History filesystem: %u total, %u used bytes\n",
                unsigned(LittleFS.totalBytes()),unsigned(LittleFS.usedBytes()));
}
bool eraseTemperatureHistory() {
  if (!historyStorageReady) return false;
  const bool erased = store.erase();
  if (erased) { temperatureHistory.clear(); savedThrough=0; }
  historyStorageHealthy = erased;
  return erased;
}
void serviceTemperatureHistory() {
  // Called only by the web task, serializing history reads/writes with configuration changes.
  if (resetRequested) return;
  const DeviceConfig config = configSnapshot();
  if (memcmp(historyProbes,config.probes,sizeof(historyProbes))) {
    // Clear old labels from flash before accepting new data; retry failed erasure.
    if (historyStorageReady) {
      if (!beginFlashMaintenance(true)) { historyStorageHealthy=false; return; }
      const bool erased=eraseTemperatureHistory();
      endFlashMaintenance();
      if (!erased) return;
    }
    temperatureHistory.clear(); savedThrough=0;loaded=false;
    memcpy(historyProbes,config.probes,sizeof(historyProbes));
  }
  const time_t wall = time(nullptr);
  if (wall<1704067200 || wall>=4102444800LL) return;
  const uint32_t now=uint32_t(wall);
  if (!loaded) {
    if (historyStorageReady) store.load(temperatureHistory,now,historyProbes);
    savedThrough=temperatureHistory.size()?temperatureHistory.at(temperatureHistory.size()-1).epoch:0;
    loaded=true;
  }
  const DeviceStatus status=statusSnapshot();
  if (status.lastReading) temperatureHistory.record(now,status.temperatures[0],status.temperatures[1],
                                                     millis()-status.lastReading<=Control::StaleAfterMs,
                                                     config.targets,status.lampPower,config.targetsSet);
  temperatureHistory.prune(now);
  if (!historyStorageReady || !temperatureHistory.size()) return;
  // If the wall clock moved backwards, allow the corrected timeline to be saved again.
  if (savedThrough>now) savedThrough=now-now%60-60;
  if (attempted && uint32_t(millis()-lastAttempt)<10000) return;
  for (size_t i=0;i<temperatureHistory.size();++i) {
    const auto& sample=temperatureHistory.at(i);
    if (sample.epoch<=savedThrough) continue;
    lastAttempt=millis();attempted=true;
    if (!beginFlashMaintenance(true)) { historyStorageHealthy=false; return; }
    const bool saved=store.save(sample,historyProbes);
    endFlashMaintenance();
    historyStorageHealthy=saved;
    if (saved) savedThrough=sample.epoch;
    return; // At most one short save per pass; failed writes retry after ten seconds.
  }
}
