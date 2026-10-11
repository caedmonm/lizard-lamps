#pragma once
#include <string>
#include <sstream>
#include <iomanip>
#include <map>
#include <vector>
#include <functional>
#include <cmath>
#include <cstring>
#include <cstdint>
#include <stdexcept>
#include <iostream>
using std::isfinite;
class String : public std::string {
public:
 using std::string::string;
 using std::string::operator=;
 String() = default;
 String(const std::string& v):std::string(v){}
 String(int v):std::string(std::to_string(v)){}
 String(float v, int precision = 2) { std::ostringstream s; s << std::fixed << std::setprecision(precision) << v; assign(s.str()); }
 bool isEmpty() const { return empty(); }
 void toCharArray(char* b, size_t n) const { if(n) { strncpy(b,c_str(),n-1); b[n-1]=0; } }
};
#define PROGMEM
#define FPSTR(x) x
using SemaphoreHandle_t = int;
constexpr int portMAX_DELAY=0, pdPASS=1;
inline void xSemaphoreTake(int,int){}
inline void xSemaphoreGive(int){}
inline int xTaskCreatePinnedToCore(void(*)(void*),const char*,int,void*,int,void*,int){return 1;}
inline unsigned long millis(){return 100;}
inline void delay(int){}
struct SerialType { template<class...T> void printf(const char*,T...){} void println(const char*){} }; inline SerialType Serial;
struct ESPType { uint64_t getEfuseMac(){return 123;} void restart(){} }; inline ESPType ESP;
inline uint32_t esp_random(){return 123;}
struct IP { String toString() const{return "192.168.1.84";} };
constexpr int WIFI_AP_STA=3,WIFI_STA=1,WL_CONNECTED=1,HTTP_GET=0,HTTP_POST=1,DIGEST_AUTH=1;
struct WiFiType { bool connected=true; void persistent(bool){} void mode(int){} void setAutoReconnect(bool){} void setHostname(const char*){} bool openAP=false; bool softAP(const char*,const char* password=nullptr){openAP=password==nullptr;return true;} void softAPdisconnect(bool){} void disconnect(bool,bool){} void begin(const char*,const char*){} int status(){return connected?WL_CONNECTED:0;} IP localIP(){return{};} IP softAPIP(){return{};} }; inline WiFiType WiFi;
inline void configTzTime(const char*,const char*,const char*){}
struct DNSServer { void start(int,const char*,IP){} void stop(){} void processNextRequest(){} };
struct Preferences {
 inline static std::map<std::string,std::vector<uint8_t>> bytes;
 inline static bool writesSucceed=true;
 bool begin(const char*,bool){return true;}
 bool clear(){bytes.clear();return true;}
 bool remove(const char* key){return bytes.erase(key)>0;}
 void end(){}
 size_t getBytesLength(const char* key){return bytes[key].size();}
 size_t getBytes(const char* key,void* out,size_t length){size_t count=std::min(length,bytes[key].size());if(count)memcpy(out,bytes[key].data(),count);return count;}
 size_t putBytes(const char* key,const void* data,size_t length){if(!writesSucceed)return 0;const auto* first=static_cast<const uint8_t*>(data);bytes[key]={first,first+length};return length;}
 String getString(const char*,const char*){return "0123456789abcdef";}
 size_t putString(const char*,const String& v){return v.size();}
};

struct WebServer {
 std::map<std::string,String> args;
 std::map<std::string,std::function<void()>> routes;
 std::map<std::string,String> headers;
 String body, contentType; int code=0; bool auth=true;
 WebServer(int){}
 String arg(const char* key){return args[key];}
 bool hasArg(const char* key){return args.count(key);}
 bool authenticate(const char*,const char*){return auth;}
 void requestAuthentication(int,const char*,const char*){code=401;body="Authenticate";}
 void sendHeader(const char* k,const String& v){headers[k]=v;}
 void send(int c,const char* type,const String& b){code=c;body=b;contentType=type;}
 template<class F> void on(const char* k,int,F f){routes[k]=f;}
 template<class F> void onNotFound(F){}
 void begin(){throw std::runtime_error("routes ready");}
 void handleClient(){}
};

struct MDNSType { bool started=false; bool begin(const char*){started=true;return true;} void end(){started=false;} void addService(const char*,const char*,int){} }; inline MDNSType MDNS;
