#pragma once
#include "TestPlatform.h"
constexpr int U_FLASH=0;
struct UpdateType {
 bool beginOK=true,writeOK=true,endOK=true,running=false;
 size_t expected=0,written=0;int begins=0,ends=0,aborts=0;
 std::function<void()> onFlash;
 bool begin(size_t size,int){++begins;if(onFlash)onFlash();expected=size;written=0;return running=beginOK;}
 size_t write(uint8_t*,size_t size){if(onFlash)onFlash();if(!writeOK)return 0;written+=size;return size;}
 bool end(){++ends;if(onFlash)onFlash();running=false;return endOK&&written==expected;}
 void abort(){++aborts;running=false;}
};
inline UpdateType Update;
