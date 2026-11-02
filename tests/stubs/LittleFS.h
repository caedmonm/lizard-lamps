#pragma once
#include "TestPlatform.h"
#include <algorithm>
#include <memory>
namespace FakeFS {
inline std::map<std::string,std::vector<uint8_t>> files;
inline bool writes=true,removes=true,mount=true;
inline size_t writeLimit=SIZE_MAX;
inline unsigned formats=0;
inline std::function<void()> onWrite;
}
class File {
 std::string name;size_t cursor=0;bool opened=false,writable=false;
public:
 File()=default;
 File(const char* path,const char* mode):name(path),writable(std::string(mode)!="r") {
  if (std::string(mode)=="w") {if(!FakeFS::writes)return;FakeFS::files[name].clear();}
  opened=FakeFS::files.count(name)>0;
 }
 operator bool() const {return opened;}
 size_t read(uint8_t* out,size_t n) {
  if(!opened)return 0;
  auto& bytes=FakeFS::files[name];if(cursor>=bytes.size())return 0;
  n=std::min(n,bytes.size()-cursor);memcpy(out,bytes.data()+cursor,n);cursor+=n;return n;
 }
 size_t write(const uint8_t* in,size_t n) {
  if(!opened||!writable||!FakeFS::writes)return 0;
  if(FakeFS::onWrite)FakeFS::onWrite();
  n=std::min(n,FakeFS::writeLimit);auto& bytes=FakeFS::files[name];bytes.resize(std::max(bytes.size(),cursor+n));
  memcpy(bytes.data()+cursor,in,n);cursor+=n;return n;
 }
 bool seek(uint32_t offset){if(!opened)return false;cursor=offset;return true;}
 void close(){opened=false;}
 void flush(){}
};
struct LittleFSType {
 bool begin(bool){return FakeFS::mount;}
 bool format(){++FakeFS::formats;FakeFS::mount=true;return true;}
 size_t totalBytes(){return 0x160000;}
 size_t usedBytes(){size_t n=0;for(auto& item:FakeFS::files)n+=item.second.size();return n;}
 File open(const char* path,const char* mode){return File(path,mode);}
 bool exists(const char* path){return FakeFS::files.count(path);}
 bool remove(const char* path){return FakeFS::removes&&FakeFS::files.erase(path);}
};
inline LittleFSType LittleFS;
