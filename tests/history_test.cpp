#include "../TemperatureHistory.h"
#include <cassert>
#include <iostream>
int main() {
  TemperatureHistory history;
  assert(history.size()==0);
  assert(history.record(750,30,24,true));
  assert(!history.record(750+TemperatureHistory::IntervalMs-1,31,25,true));
  assert(history.record(750+TemperatureHistory::IntervalMs,31,25,true));
  assert(history.at(0).temperatures[0]==30 && history.at(1).temperatures[1]==25);
  history.clear();
  for(size_t i=0;i<TemperatureHistory::Capacity+100;++i)
    assert(history.record(uint32_t(i*TemperatureHistory::IntervalMs),float(i),22,true));
  assert(history.size()==577);
  assert(history.at(0).uptime==100*TemperatureHistory::IntervalMs);
  assert(history.at(576).uptime==676*TemperatureHistory::IntervalMs);
  history.prune(history.at(576).uptime+TemperatureHistory::RetentionMs+1);
  assert(history.size()==0);
  history.clear();
  const uint32_t start=UINT32_MAX-1000;
  history.record(start,30,24,true);
  assert(!history.record(start+TemperatureHistory::IntervalMs-1,30,24,true));
  assert(history.record(start+TemperatureHistory::IntervalMs,-127,46,true));
  assert(std::isnan(history.at(1).temperatures[0]));
  assert(history.at(1).temperatures[1]==46); // Keep over-limit data visible.
  history.record(start+2*TemperatureHistory::IntervalMs,30,24,false);
  assert(std::isnan(history.at(2).temperatures[0])&&std::isnan(history.at(2).temperatures[1]));
  history.record(start+3*TemperatureHistory::IntervalMs,NAN,INFINITY,true);
  assert(std::isnan(history.at(3).temperatures[0])&&std::isnan(history.at(3).temperatures[1]));
  // Delays produce real gaps, never fabricated backfilled measurements.
  history.record(start+TemperatureHistory::RetentionMs,29,23,true);
  assert(history.size()==5);
  history.prune(start+TemperatureHistory::RetentionMs+1);
  assert(history.size()==4);
  std::cout<<"History cadence, 48h eviction, bounded storage, gaps, invalid readings and uptime rollover passed ("<<sizeof(history)<<" bytes).\n";
}
