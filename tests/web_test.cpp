#include "../DeviceWeb.cpp"
#include <fstream>
#include <cassert>
DeviceConfig deviceConfig;
DeviceStatus deviceStatus;
SemaphoreHandle_t stateMutex=1;
bool maintenance=false,maintenanceAcknowledged=false,controllerRunning=false,resetRequested=false;
DeviceConfig configSnapshot(){return deviceConfig;}
DeviceStatus statusSnapshot(){return deviceStatus;}
bool saveSucceeds = true;
bool saveConfig(const DeviceConfig& c){if (!saveSucceeds) return false;deviceConfig=c;return true;}
bool loadConfig(){return true;}
int main(int argc, char** argv){
 const std::string outputDirectory = argc > 1 ? argv[1] : "/tmp";
 try{webTask(nullptr);}catch(const std::runtime_error&){}
 csrf="test-token"; hotspot=false;
 render(); std::ofstream(outputDirectory + "/setup.html")<<server.body;
 assert(server.body.find("Choose a dashboard password")!=String::npos);
 server.args={{"csrf","wrong"}}; server.routes["/targets"](); assert(server.code==403);
 server.args={{"csrf",csrf},{"ssid","Enclosure Wi-Fi"},{"wifiPassword","password123"},{"adminPassword","dashboard123"}};
 server.routes["/wifi"](); assert(server.code==303 && String(deviceConfig.ssid)=="Enclosure Wi-Fi");
 server.routes["/connecting"]();assert(server.body.find("192.168.1.84")!=String::npos);
 std::ofstream(outputDirectory+"/connecting.html")<<server.body;
 WiFi.connected=false;server.routes["/connecting"]();assert(server.body.find("Check Wi-Fi details")!=String::npos);WiFi.connected=true;
 server.auth=false; server.routes["/"](); assert(server.code==401); server.auth=true;
 server.args={{"csrf",csrf},{"basking","nan"},{"ambient","25"}}; server.routes["/targets"](); assert(server.code==422 && !deviceConfig.targetsSet);
 server.args={{"csrf",csrf},{"basking","30"},{"ambient","24"}}; server.routes["/targets"](); assert(server.code==303 && deviceConfig.targetsSet);
 server.args={{"csrf",csrf},{"lamp1","07:00"},{"lamp2","07:00"}}; server.routes["/schedule"](); assert(server.code==422 && !deviceConfig.scheduleSet);
 saveSucceeds=false;
 server.args={{"csrf",csrf},{"lamp1","07:00"},{"lamp2","19:00"}};server.routes["/schedule"]();assert(server.code==500&&!deviceConfig.scheduleSet);
 saveSucceeds=true;
 server.args={{"csrf",csrf},{"lamp1","07:00"},{"lamp2","19:00"}}; server.routes["/schedule"](); assert(server.code==303 && deviceConfig.scheduleSet);
 deviceStatus.temperatures[0]=29.4f;deviceStatus.temperatures[1]=23.1f;
 deviceStatus.sensorsHealthy=true;deviceStatus.dimmerOnline=true;deviceStatus.clockValid=true;deviceStatus.power=30;deviceStatus.activeLamp=0;
 deviceConfig.probesSet=true;
 for(int i=0;i<8;++i){deviceConfig.probes[0][i]=i+1;deviceConfig.probes[1][i]=i+11;}
 for(const char* view:{"monitor","settings","schedule","device"}){
   server.args={{"view",view}};render();std::ofstream(outputDirectory+"/"+view+".html")<<server.body;
 }
 server.args={{"csrf",csrf}};server.routes["/probes"]();assert(deviceConfig.probes[0][0]==11);
 strcpy(deviceConfig.ssid,"<script>alert(1)</script>");server.args={{"view","device"}};render();assert(server.body.find("&lt;script&gt;alert(1)&lt;/script&gt;")!=String::npos);
 server.args={{"csrf",csrf},{"confirmation","no"}};server.routes["/reset"]();assert(server.code==422&&!resetRequested);
 server.args={{"csrf",csrf},{"confirmation","RESET"}};server.routes["/reset"]();assert(server.code==200&&resetRequested);
 std::cout<<"Actual firmware pages rendered; authentication, CSRF, validation, saving, probe mapping, escaping and reset confirmation checks passed with mocked hardware.\n";
}
