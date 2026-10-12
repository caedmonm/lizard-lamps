#include "../DeviceState.cpp"
#include <cassert>
#include <iostream>
template<class Config> void store(const Config& config) {
  const auto* first=reinterpret_cast<const uint8_t*>(&config);
  Preferences::bytes["config"]={first,first+sizeof(config)};
}
int main() {
  stateMutex=1;
  LegacyDeviceConfigV1 old;
  strcpy(old.ssid,"Home Wi-Fi");strcpy(old.wifiPassword,"wifi-password");strcpy(old.adminPassword,"dashboard-password");
  old.targets[0]=31;old.targets[1]=25;old.starts[0]=480;old.starts[1]=1200;
  old.targetsSet=old.scheduleSet=old.probesSet=true;
  for(int probe=0;probe<2;++probe){old.probes[probe][0]=0x28;old.probes[probe][1]=probe+1;old.probes[probe][7]=OneWire::crc8(old.probes[probe],7);}
  store(old);assert(loadConfig());
  assert(deviceConfig.version==3&&deviceConfig.scheduleSet&&deviceConfig.targetsSet);
  assert(!strcmp(deviceConfig.ssid,old.ssid)&&!strcmp(deviceConfig.wifiPassword,old.wifiPassword)&&!strcmp(deviceConfig.adminPassword,old.adminPassword));
  assert(!memcmp(deviceConfig.probes,old.probes,sizeof(old.probes)));
  assert(deviceConfig.starts[0]==480&&deviceConfig.starts[1]==1200);
  for(int period=0;period<2;++period){assert(deviceConfig.targets[period][0]==31&&deviceConfig.targets[period][1]==25);}
  assert(Preferences::bytes["config"].size()==sizeof(DeviceConfig));
  deviceConfig.targets[1][0]=20;deviceConfig.targets[1][1]=18;assert(saveConfig(deviceConfig));
  deviceConfig=DeviceConfig{};assert(loadConfig());assert(deviceConfig.targets[0][0]==31&&deviceConfig.targets[1][0]==20);
  old.scheduleSet=false;store(old);assert(loadConfig());assert(!deviceConfig.scheduleSet&&!deviceConfig.targetsSet);
  old.scheduleSet=true;store(old);Preferences::writesSucceed=false;
  assert(loadConfig());assert(deviceConfig.targets[1][0]==31&&Preferences::bytes["config"].size()==sizeof(old));
  Preferences::writesSucceed=true;assert(loadConfig());assert(Preferences::bytes["config"].size()==sizeof(DeviceConfig));
  LegacyDeviceConfigV2 previous;
  memcpy(previous.ssid, old.ssid, sizeof(previous.ssid));
  memcpy(previous.wifiPassword, old.wifiPassword, sizeof(previous.wifiPassword));
  memcpy(previous.adminPassword, old.adminPassword, sizeof(previous.adminPassword));
  memcpy(previous.probes, old.probes, sizeof(previous.probes));
  previous.targets[0][0]=30;previous.targets[0][1]=24;
  previous.targets[1][0]=20;previous.targets[1][1]=18;
  previous.starts[0]=500;previous.starts[1]=1100;
  previous.targetsSet=previous.scheduleSet=previous.probesSet=true;
  store(previous);assert(loadConfig());
  assert(deviceConfig.version==3&&deviceConfig.dimmingEnabled&&deviceConfig.dimmingRange==3&&deviceConfig.restartDrop==2);
  assert(!memcmp(deviceConfig.targets,previous.targets,sizeof(previous.targets)));
  assert(!memcmp(deviceConfig.probes,previous.probes,sizeof(previous.probes)));
  assert(!strcmp(deviceConfig.ssid,previous.ssid)&&!strcmp(deviceConfig.wifiPassword,previous.wifiPassword)&&!strcmp(deviceConfig.adminPassword,previous.adminPassword));
  assert(!memcmp(deviceConfig.starts,previous.starts,sizeof(previous.starts)));
  deviceConfig.dimmingEnabled=false;deviceConfig.dimmingRange=4.5f;deviceConfig.restartDrop=1.5f;
  assert(saveConfig(deviceConfig));deviceConfig=DeviceConfig{};assert(loadConfig());
  assert(!deviceConfig.dimmingEnabled&&deviceConfig.dimmingRange==4.5f&&deviceConfig.restartDrop==1.5f);
  DeviceConfig badRange=deviceConfig;badRange.dimmingRange=0;assert(!saveConfig(badRange));
  badRange=deviceConfig;badRange.restartDrop=NAN;store(badRange);assert(!loadConfig());
  badRange=deviceConfig;badRange.restartDrop=11;assert(!saveConfig(badRange));
  previous.version=99;store(previous);assert(!loadConfig());
  DeviceConfig invalid=deviceConfig;invalid.targets[1][1]=NAN;store(invalid);assert(!loadConfig());
  invalid=deviceConfig;invalid.targets[0][0]=41;assert(!saveConfig(invalid));

  DeviceConfig open=deviceConfig;memset(open.adminPassword,0,sizeof(open.adminPassword));assert(saveConfig(open));
  deviceConfig=DeviceConfig{};assert(loadConfig());assert(deviceConfig.ssid[0]&&!deviceConfig.adminPassword[0]);
  invalid=deviceConfig;strcpy(invalid.adminPassword,"short");assert(!saveConfig(invalid));
  old.targets[0]=NAN;store(old);assert(!loadConfig());
  old.targets[0]=31;old.version=99;store(old);assert(!loadConfig());
  std::cout<<"Legacy migration, preserved credentials/probes, persistence, incomplete setup and invalid targets passed with mocked storage.\n";
}
