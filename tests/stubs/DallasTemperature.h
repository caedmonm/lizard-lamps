#pragma once
#include "OneWire.h"
struct DallasTemperature {
  float readings[2] = {31.5f, 21.8f};
  explicit DallasTemperature(OneWire*){}
  void begin(){}
  void setResolution(int){}
  void setWaitForConversion(bool){}
  void requestTemperatures(){}
  int getDeviceCount(){return 0;}
  bool getAddress(uint8_t*,int){return false;}
  float getTempC(const uint8_t* address){return readings[address[1]];}
};
