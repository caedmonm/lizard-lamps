#pragma once
namespace Mycila {
struct PulseAnalyzer {
  bool online = true;
  void onZeroCross(void(*)()){}
  bool begin(int){return true;}
  bool isOnline(){return online;}
};
}
