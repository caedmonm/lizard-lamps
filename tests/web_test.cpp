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
 assert(WiFi.openAP);
 csrf="test-token"; hotspot=false;
 render(); std::ofstream(outputDirectory + "/setup.html")<<server.body;
 assert(server.body.find("Choose a dashboard password")!=String::npos);
 server.args={{"csrf","wrong"}}; server.routes["/schedule"](); assert(server.code==403);
 server.args={{"csrf",csrf},{"ssid","Enclosure Wi-Fi"},{"wifiPassword","password123"},{"adminPassword","dashboard123"}};
 server.routes["/wifi"](); assert(server.code==303 && String(deviceConfig.ssid)=="Enclosure Wi-Fi");
 networkTick();assert(MDNS.started);
 server.routes["/connecting"]();assert(server.body.find("192.168.1.84")!=String::npos);
 assert(server.body.find("http://lizard-lamps.local")!=String::npos);
 assert(server.body.find("http://192.168.4.1")!=String::npos);
 std::ofstream(outputDirectory+"/connecting.html")<<server.body;
 WiFi.connected=false;networkTick();assert(!MDNS.started);server.routes["/connecting"]();assert(server.body.find("Check Wi-Fi details")!=String::npos);WiFi.connected=true;
 server.auth=false; server.routes["/"](); assert(server.code==401); server.auth=true;
 auto scheduleArgs = [&] {
   server.args={{"csrf",csrf},{"lamp1","07:00"},{"lamp2","19:00"},
                {"dayBasking","30"},{"dayAmbient","24"},{"nightBasking","20"},{"nightAmbient","18"}};
 };
 scheduleArgs();server.args["nightAmbient"]="nan";server.routes["/schedule"]();assert(server.code==422&&!deviceConfig.scheduleSet);
 scheduleArgs();server.args["nightBasking"]="41";server.routes["/schedule"]();assert(server.code==422&&!deviceConfig.scheduleSet);
 scheduleArgs();server.args.erase("dayAmbient");server.routes["/schedule"]();assert(server.code==422&&!deviceConfig.targetsSet);
 scheduleArgs();server.args["lamp2"]="07:00";server.routes["/schedule"]();assert(server.code==422&&!deviceConfig.scheduleSet);
 saveSucceeds=false;scheduleArgs();server.routes["/schedule"]();assert(server.code==500&&!deviceConfig.scheduleSet);
 saveSucceeds=true;scheduleArgs();server.routes["/schedule"]();assert(server.code==303&&deviceConfig.scheduleSet&&deviceConfig.targetsSet);
 assert(deviceConfig.targets[0][0]==30&&deviceConfig.targets[0][1]==24&&deviceConfig.targets[1][0]==20&&deviceConfig.targets[1][1]==18);
 assert(server.routes.count("/targets")==0);
 deviceStatus.temperatures[0]=29.4f;deviceStatus.temperatures[1]=23.1f;
 deviceStatus.sensorsHealthy=true;deviceStatus.dimmerOnline=true;deviceStatus.clockValid=true;deviceStatus.power=30;deviceStatus.activeLamp=0;deviceStatus.targets[0]=30;deviceStatus.targets[1]=24;
 deviceConfig.probesSet=true;
 for(int i=0;i<8;++i){deviceConfig.probes[0][i]=i+1;deviceConfig.probes[1][i]=i+11;}
 for(const char* view:{"monitor","settings","schedule","device"}){
   server.args={{"view",view}};render();std::ofstream(outputDirectory+"/"+view+".html")<<server.body;
 }
 server.args={{"view","schedule"}};render();assert(server.body.find("nightBasking")!=String::npos&&server.body.find("dayAmbient")!=String::npos);
 deviceStatus.activeLamp=1;deviceStatus.targets[0]=20;deviceStatus.targets[1]=18;server.args={{"view","monitor"}};render();assert(server.body.find("Night · Lamp 2")!=String::npos&&server.body.find("20.0°C")!=String::npos);
 server.args={{"csrf",csrf}};server.routes["/probes"]();assert(deviceConfig.probes[0][0]==11);
 strcpy(deviceConfig.ssid,"<script>alert(1)</script>");server.args={{"view","device"}};render();assert(server.body.find("&lt;script&gt;alert(1)&lt;/script&gt;")!=String::npos);
 server.args={{"csrf",csrf},{"confirmation","no"}};server.routes["/reset"]();assert(server.code==422&&!resetRequested);
 server.args={{"csrf",csrf},{"confirmation","RESET"}};server.routes["/reset"]();assert(server.code==200&&resetRequested);
 std::cout<<"Actual firmware pages rendered; authentication, CSRF, validation, saving, probe mapping, escaping and reset confirmation checks passed with mocked hardware.\n";
}
