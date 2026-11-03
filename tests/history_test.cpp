#include "../TemperatureHistory.h"
#include <cassert>
#include <iostream>
int main() {
  TemperatureHistory history;
  const uint32_t start=1704067200;
  const float targets[2][2]={{30,24},{20,18}};const int power[2]={63,0};
  auto record=[&](uint32_t now,float a=30,float b=24,bool fresh=true) {
    return history.record(now,a,b,fresh,targets,power);
  };
  assert(!record(0));assert(record(start));assert(!record(start+59));assert(record(start+60));
  assert(history.at(0).temperatures[0]==300&&history.at(0).targets[1][0]==200&&history.at(0).power[0]==63);
  history.clear();
  for(size_t i=0;i<TemperatureHistory::Capacity+100;++i) assert(record(start+i*60));
  assert(history.size()==2881&&history.at(0).epoch==start+100*60);
  history.prune(history.at(2880).epoch+TemperatureHistory::RetentionSeconds+1);
  assert(history.size()==0);
  history.clear();record(start,-127,46);assert(history.at(0).temperatures[0]==TemperatureHistory::Missing);
  assert(TemperatureHistory::decode(history.at(0).temperatures[1])==46);
  record(start+60,30,24,false);assert(history.at(1).temperatures[0]==TemperatureHistory::Missing);
  record(start+180,NAN,INFINITY);assert(history.size()==3); // No fabricated minute during a delay.
  assert(std::isnan(TemperatureHistory::decode(history.at(2).temperatures[1])));
  assert(record(start+120));assert(history.at(history.size()-1).epoch==start+120); // Backwards clock correction.
  history.clear();history.record(start,30,24,true,targets,power,false);
  assert(history.at(0).targets[0][0]==TemperatureHistory::Missing);
  history.clear();
  history.record(start,30,24,true,targets,power,true,0);
  history.record(start+60,20,18,true,targets,power,true,1);
  history.record(start+120,15,18,true,targets,power,false,2,15);
  assert(TemperatureHistory::activeTarget(history.at(0),0)==30);
  assert(TemperatureHistory::activeTarget(history.at(0),1)==24);
  assert(TemperatureHistory::activeTarget(history.at(1),0)==20);
  assert(TemperatureHistory::activeTarget(history.at(1),1)==18);
  assert(TemperatureHistory::activeTarget(history.at(2),0)==15);
  assert(std::isnan(TemperatureHistory::activeTarget(history.at(2),1)));
  auto legacy=history.at(0);legacy.reserved=0;
  assert(std::isnan(TemperatureHistory::activeTarget(legacy,0)));
  std::cout<<"Minute history, 48h expiry, clock correction, temperatures, lamp percentages and four targets passed ("<<sizeof(history)<<" RAM bytes).\n";
}
