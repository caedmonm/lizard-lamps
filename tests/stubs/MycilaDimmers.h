#pragma once
namespace Mycila {
struct Dimmer { static void setSemiPeriod(int){} };
struct ThyristorDimmer {
  float duty = 0;
  void setPin(int){}
  void enablePowerLUT(bool){}
  bool begin(){return true;}
  void end(){}
  void setOnline(bool){}
  void setDutyCycle(float value){duty=value;}
  static void onZeroCross(){}
};
}
