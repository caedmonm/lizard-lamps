#include "../DeviceState.cpp"
#include <time.h>
time_t historyTestClock=1704067200;
time_t historyTestTime(time_t*) {return historyTestClock;}
#define time historyTestTime
#include "../HistoryPersistence.cpp"
#undef time
#include <cassert>
int main() {
 stateMutex=1;
 // Fresh partition is initialized only while outputs are not running.
 FakeFS::mount=false;startHistoryStorage();assert(historyStorageReady&&FakeFS::formats==1);
 deviceConfig.targetsSet=true;deviceConfig.probesSet=true;
 deviceStatus.lastReading=testMillis;deviceStatus.temperatures[0]=29.5;deviceStatus.temperatures[1]=23;
 deviceStatus.lampPower[0]=74;deviceStatus.activeLamp=1;deviceConfig.scheduleSet=true;
 deviceConfig.targets[1][0]=20;deviceConfig.targets[1][1]=18;
 int pauses=0;
 FakeFS::onWrite=[&] {assert(maintenance&&historyMaintenance);++pauses;};
 serviceTemperatureHistory();assert(temperatureHistory.size()==1&&historyStorageHealthy&&!maintenance&&pauses==0);
 // Keep minute resolution in RAM without touching flash for ten minutes.
 for(int minute=1;minute<10;++minute) {
   historyTestClock+=60;testMillis+=60000;deviceStatus.lastReading=testMillis;
   serviceTemperatureHistory();assert(pauses==0&&temperatureHistory.size()==size_t(minute+1));
 }
 historyTestClock+=60;testMillis+=60000;deviceStatus.lastReading=testMillis;
 serviceTemperatureHistory();assert(pauses>0&&savedThrough==uint32_t(historyTestClock)&&!maintenance);
 assert(temperatureHistory.size()==11);
 assert(temperatureHistory.at(0).power[0]==74);
 // A fresh RAM state restores from flash without requiring another measurement.
 loaded=false;temperatureHistory.clear();savedThrough=0;deviceStatus.lastReading=0;
 historyTestClock+=10;serviceTemperatureHistory();assert(temperatureHistory.size()==11);
 assert(TemperatureHistory::activeTarget(temperatureHistory.at(0),0)==20);
 assert(TemperatureHistory::activeTarget(temperatureHistory.at(0),1)==18);
 // Failed writes remain in RAM, report failure and retry without stopping normal control.
 historyTestClock+=600;testMillis+=600000;deviceStatus.lastReading=testMillis;
 FakeFS::writes=false;serviceTemperatureHistory();assert(!historyStorageHealthy&&!maintenance);
 FakeFS::writes=true;testMillis+=10000;deviceStatus.lastReading=testMillis;
 serviceTemperatureHistory();assert(historyStorageHealthy);
 // A backwards clock correction may replace the latest minute, never future-dated points.
 historyTestClock-=60;testMillis+=10000;serviceTemperatureHistory();
 assert(temperatureHistory.at(temperatureHistory.size()-1).epoch<=uint32_t(historyTestClock));
 // Recover a long persistence outage in bounded batches, rather than one long off period.
 FakeFS::writes=false;
 for(int minute=0;minute<35;++minute) {
   historyTestClock+=60;testMillis+=60000;deviceStatus.lastReading=testMillis;
   serviceTemperatureHistory();
 }
 FakeFS::writes=true;testMillis+=10000;deviceStatus.lastReading=testMillis;
 const uint32_t beforeCatchup=savedThrough;
 serviceTemperatureHistory();assert(historyStorageHealthy&&flushPending&&savedThrough>beforeCatchup);
 assert(savedThrough-beforeCatchup<=16*60);
 for(int batch=0;batch<3&&flushPending;++batch){testMillis+=10000;deviceStatus.lastReading=testMillis;serviceTemperatureHistory();}
 assert(!flushPending&&savedThrough==uint32_t(historyTestClock)-uint32_t(historyTestClock)%60);
 // Probe swaps erase old identities before a new record is saved.
 ++deviceConfig.probes[0][1];testMillis+=10000;deviceStatus.lastReading=testMillis;
 serviceTemperatureHistory();assert(temperatureHistory.size()==1);
 historyTestClock=0;serviceTemperatureHistory();assert(temperatureHistory.size()==1); // Wait for valid time, preserve saved data.
 resetRequested=true;const size_t count=temperatureHistory.size();serviceTemperatureHistory();assert(temperatureHistory.size()==count);resetRequested=false;
 assert(eraseTemperatureHistory()&&FakeFS::files.empty());
 // Existing damaged/unknown filesystems are never silently formatted.
 historyStorageReady=false;historyStorageHealthy=false;
 FakeFS::mount=false;fakePartitionBlank=false;startHistoryStorage();assert(!historyStorageReady&&FakeFS::formats==1);
 controllerRunning=true;testMillis=0;
 assert(!beginFlashMaintenance(true));assert(!maintenance&&!historyMaintenance); // Controller never acknowledged off.
 std::cout<<"History startup, restore, guarded saves, failed-write retry, probe swap, invalid clock and maintenance timeout passed.\n";
}
