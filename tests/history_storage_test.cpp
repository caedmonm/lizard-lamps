#include "../HistoryStorage.h"
#include <cassert>
#include <iostream>
int main() {
 HistoryStorage store;TemperatureHistory history;
 const uint32_t start=1704067200;uint8_t probes[2][8]={{0x28,1},{0x28,2}};
 float targets[2][2]={{30,24},{20,18}};int powers[2]={72,0};
 for(unsigned i=0;i<3001;++i) {
  history.record(start+i*60,29.4f,23.1f,true,targets,powers);
  assert(store.save(history.at(history.size()-1),probes));
 }
 assert(FakeFS::files.size()==49);assert(LittleFS.usedBytes()==49*(28+60*24));
 TemperatureHistory restored;store.load(restored,start+3000*60,probes);
 assert(restored.size()==2881&&restored.at(0).epoch==start+120*60);
 assert(restored.at(0).temperatures[0]==294&&restored.at(0).power[0]==72&&restored.at(0).targets[1][1]==180);
 // Simulate a reboot with a truncated record and a corrupt record, retaining all other points.
 char name[24];HistoryStorage::path((start/3600+50)%49,name);
 auto& file=FakeFS::files[name];file.resize(28+10); // Current hour's only sample is incomplete.
 HistoryStorage::path((start/3600+49)%49,name);FakeFS::files[name][28+4]^=1;
 store.load(restored,start+3000*60,probes);assert(restored.size()==2879);
 auto swapped=probes[0][1];probes[0][1]=99;
 store.load(restored,start+3000*60,probes);assert(restored.size()==0);probes[0][1]=swapped;
 store.load(restored,start+3000*60+49*3600,probes);assert(restored.size()==0);
 // Partial writes report failure and are rejected when restored.
 history.record(start+3001*60,31,25,true,targets,powers);
 FakeFS::writeLimit=8;assert(!store.save(history.at(history.size()-1),probes));FakeFS::writeLimit=SIZE_MAX;
 FakeFS::removes=false;assert(!store.erase());FakeFS::removes=true;assert(store.erase());assert(FakeFS::files.empty());
 std::cout<<"Persistent history reboot recovery, bounded hourly files, retention, probe identities, corruption and failed writes passed.\n";
}
