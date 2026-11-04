#pragma once
#include "TemperatureHistory.h"
#include <LittleFS.h>
#include <cstring>

// Keep the original 49-slot addressing so upgrades can read existing files.
// Prune expired files on saves, retaining at most 25 hourly files for 24 hours.
// CRCs reject incomplete records after an interrupted write; no native struct padding is persisted.
class HistoryStorage {
public:
  static constexpr unsigned Slots = 49;
  static constexpr unsigned RetainedHours = TemperatureHistory::RetentionSeconds / 3600;
  struct Header { uint32_t magic, hour; uint8_t probes[2][8]; uint32_t crc; };
  struct Record { TemperatureHistory::Sample sample; uint32_t crc; };
  static_assert(sizeof(Header) == 28 && sizeof(Record) == 24, "Disk format changed");
  static uint32_t checksum(const void* data, size_t length) {
    uint32_t crc = 0xffffffff;
    const auto* bytes = static_cast<const uint8_t*>(data);
    for (size_t i=0; i<length; ++i) {
      crc ^= bytes[i];
      for (int bit=0; bit<8; ++bit) crc = (crc >> 1) ^ (0xedb88320UL & (0U - (crc & 1)));
    }
    return ~crc;
  }
  static void path(unsigned slot, char* out) { snprintf(out, 24, "/temps-%02u.bin", slot); }
  static bool header(File& file, Header& h) {
    return file.read(reinterpret_cast<uint8_t*>(&h), sizeof(h)) == sizeof(h) &&
           h.magic == 0x4c4c5432 && h.crc == checksum(&h, sizeof(h)-4);
  }
  bool load(TemperatureHistory& history, uint32_t now, const uint8_t probes[2][8]) {
    history.clear();
    const uint32_t hour = now / 3600;
    for (uint32_t bucket=hour-RetainedHours; bucket<=hour; ++bucket) {
      char name[24]; path(bucket % Slots, name);
      File file = LittleFS.open(name, "r");
      Header h;
      if (!file || !header(file,h) || h.hour != bucket || memcmp(h.probes,probes,16)) continue;
      // Fixed minute slots make ordering independent of resets or missing samples.
      for (unsigned minute=0; minute<60; ++minute) {
        Record record;
        if (file.read(reinterpret_cast<uint8_t*>(&record),sizeof(record)) != sizeof(record)) break;
        const auto& sample=record.sample;
        if (record.crc != checksum(&sample,sizeof(sample)) || !TemperatureHistory::validClock(sample.epoch) ||
            sample.epoch != bucket*3600+minute*60 || sample.epoch>now ||
            now-sample.epoch>TemperatureHistory::RetentionSeconds) continue;
        history.append(sample);
      }
    }
    return true;
  }
  // Called inside the flash-maintenance guard, once per hour (or after failure).
  // Preserve the on-disk mapping; changing the modulus would hide saved readings.
  bool prune(uint32_t now) {
    if (!TemperatureHistory::validClock(now)) return false;
    const uint32_t hour=now/3600;
    if (prunedHour==hour) return true;
    bool ok=true;
    for (unsigned slot=0;slot<Slots;++slot) {
      char name[24];path(slot,name);
      if (!LittleFS.exists(name)) continue;
      File file=LittleFS.open(name,"r");
      if (!file) { ok=false;continue; }
      Header h;
      const bool expired=!header(file,h) || h.hour<hour-RetainedHours || h.hour>hour;
      file.close();
      if (expired && !LittleFS.remove(name)) ok=false;
    }
    if (ok) prunedHour=hour;
    return ok;
  }
  bool save(const TemperatureHistory::Sample& sample, const uint8_t probes[2][8]) {
    char name[24]; path((sample.epoch/3600) % Slots, name);
    Header h;
    File file=LittleFS.open(name,"r+");
    bool existing=file && header(file,h) && h.hour==sample.epoch/3600 && !memcmp(h.probes,probes,16);
    if (!existing) {
      file.close();file=LittleFS.open(name,"w");
      if (!file) return false;
      h={};h.magic=0x4c4c5432;h.hour=sample.epoch/3600;memcpy(h.probes,probes,16);
      h.crc=checksum(&h,sizeof(h)-4);
      Record empty[60] = {};
      if (file.write(reinterpret_cast<const uint8_t*>(&h),sizeof(h))!=sizeof(h) ||
          file.write(reinterpret_cast<const uint8_t*>(empty),sizeof(empty))!=sizeof(empty)) return false;
    }
    Record record={sample,checksum(&sample,sizeof(sample))};
    const uint32_t offset=sizeof(Header)+(sample.epoch%3600/60)*sizeof(Record);
    if (!file.seek(offset) || file.write(reinterpret_cast<const uint8_t*>(&record),sizeof(record))!=sizeof(record)) return false;
    file.flush();file.close();
    file=LittleFS.open(name,"r");
    Record verified;
    return file && file.seek(offset) && file.read(reinterpret_cast<uint8_t*>(&verified),sizeof(verified))==sizeof(verified) &&
           !memcmp(&record,&verified,sizeof(record));
  }
  bool erase() {
    prunedHour=0;
    bool ok=true;
    for (unsigned slot=0;slot<Slots;++slot) {
      char name[24];path(slot,name);
      if (LittleFS.exists(name) && !LittleFS.remove(name)) ok=false;
    }
    return ok;
  }
private:
  uint32_t prunedHour=0;
};
