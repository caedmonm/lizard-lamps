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
  assert(deviceConfig.version==2&&deviceConfig.scheduleSet&&deviceConfig.targetsSet);
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
  DeviceConfig invalid=deviceConfig;invalid.targets[1][1]=NAN;store(invalid);assert(!loadConfig());
  invalid=deviceConfig;invalid.targets[0][0]=41;assert(!saveConfig(invalid));

  DeviceConfig open=deviceConfig;memset(open.adminPassword,0,sizeof(open.adminPassword));assert(saveConfig(open));
  deviceConfig=DeviceConfig{};assert(loadConfig());assert(deviceConfig.ssid[0]&&!deviceConfig.adminPassword[0]);
  invalid=deviceConfig;strcpy(invalid.adminPassword,"short");assert(!saveConfig(invalid));
  old.targets[0]=NAN;store(old);assert(!loadConfig());
  old.targets[0]=31;old.version=99;store(old);assert(!loadConfig());
  std::cout<<"Legacy migration, preserved credentials/probes, persistence, incomplete setup and invalid targets passed with mocked storage.\n";
}
