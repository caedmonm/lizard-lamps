#pragma once
#include "TestPlatform.h"
struct OneWire {
 static uint8_t crc8(const uint8_t* bytes, uint8_t length) {
   uint8_t crc=0;
   for (uint8_t i=0;i<length;++i) {
     uint8_t value=bytes[i];
     for (uint8_t bit=0;bit<8;++bit) {
       uint8_t mix=(crc^value)&1;crc>>=1;if(mix)crc^=0x8C;value>>=1;
     }
   }
   return crc;
 }
};
