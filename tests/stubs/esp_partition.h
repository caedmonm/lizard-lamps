#pragma once
#include "TestPlatform.h"
constexpr int ESP_PARTITION_TYPE_DATA=1,ESP_PARTITION_SUBTYPE_DATA_SPIFFS=0x82,ESP_OK=0;
struct esp_partition_t { size_t size=0x160000; };
inline esp_partition_t fakePartition;
inline bool fakePartitionBlank=true,fakePartitionPresent=true;
inline const esp_partition_t* esp_partition_find_first(int,int,const char*) {return fakePartitionPresent?&fakePartition:nullptr;}
inline int esp_partition_read(const esp_partition_t*,size_t,void* out,size_t length) {
 memset(out,fakePartitionBlank?0xff:0,length);return ESP_OK;
}
