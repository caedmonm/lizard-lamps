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
 server.routes["/api/status"]();assert(server.body.find("null")!=String::npos);assert(server.contentType.find("application/json")!=String::npos);
 std::ofstream(outputDirectory+"/status-empty.json")<<server.body;

 render(); std::ofstream(outputDirectory + "/setup.html")<<server.body;
 assert(server.body.find("Choose a dashboard password")!=String::npos);
 server.args={{"csrf","wrong"}}; server.routes["/schedule"](); assert(server.code==403);
 server.args={{"csrf",csrf},{"ssid","Enclosure Wi-Fi"},{"protectAdmin","1"}};
 server.routes["/api/wifi"]();assert(server.code==422&&!deviceConfig.ssid[0]);
 server.args={{"csrf",csrf},{"ssid","Enclosure Wi-Fi"},{"wifiPassword","password123"},{"adminPassword","dashboard123"},{"protectAdmin","1"}};
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
 auto controlArgs = [&] { server.args={{"csrf",csrf},{"dimmingEnabled","0"},{"dimmingRange","4"},{"restartDrop","2"}}; };
 server.auth=false;controlArgs();server.routes["/api/control"]();assert(server.code==401&&deviceConfig.dimmingEnabled);server.auth=true;
 controlArgs();server.args["csrf"]="wrong";server.routes["/api/control"]();assert(server.code==403&&deviceConfig.dimmingEnabled);
 for(const char* field:{"dimmingEnabled","restartDrop"}){
   controlArgs();server.args.erase(field);server.routes["/api/control"]();assert(server.code==422&&deviceConfig.dimmingEnabled);
 }
 for(const char* bad:{"nan","inf","0","-2","10.1","2junk"}){
   controlArgs();server.args["restartDrop"]=bad;server.routes["/api/control"]();assert(server.code==422&&deviceConfig.dimmingEnabled);
   controlArgs();server.args["dimmingRange"]=bad;server.routes["/api/control"]();assert(server.code==422&&deviceConfig.dimmingEnabled);
 }
 controlArgs();server.args["dimmingEnabled"]="1";server.args.erase("dimmingRange");server.routes["/api/control"]();assert(server.code==422&&deviceConfig.dimmingEnabled);
 controlArgs();server.args["dimmingEnabled"]="yes";server.routes["/api/control"]();assert(server.code==422&&deviceConfig.dimmingEnabled);
 saveSucceeds=false;controlArgs();server.routes["/api/control"]();assert(server.code==500&&deviceConfig.dimmingEnabled);saveSucceeds=true;
 controlArgs();server.routes["/api/control"]();assert(server.code==200&&!deviceConfig.dimmingEnabled&&deviceConfig.dimmingRange==4&&deviceConfig.restartDrop==2);
 controlArgs();server.args.erase("dimmingRange");server.args["restartDrop"]="1.5";server.routes["/api/control"]();assert(server.code==200&&deviceConfig.dimmingRange==4&&deviceConfig.restartDrop==1.5f);
 controlArgs();server.routes["/api/control"]();assert(server.code==200&&deviceConfig.restartDrop==2);
 assert(deviceConfig.targets[0][0]==30&&deviceConfig.starts[0]==420);
 server.args={{"view","settings"}};render();assert(server.body.find("Restart temperature drop")!=String::npos&&server.body.find("value='0' selected")!=String::npos);
 controlArgs();server.args["dimmingEnabled"]="1";server.routes["/control"]();assert(server.code==303&&deviceConfig.dimmingEnabled);
 server.routes["/api/config"]();assert(server.body.find("\"dimmingEnabled\":true")!=String::npos&&server.body.find("\"dimmingRange\":4.0")!=String::npos&&server.body.find("\"restartDrop\":2.0")!=String::npos);
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
 server.routes["/api/config"]();assert(server.body.find("wifiPassword")==String::npos&&server.body.find("adminPassword")==String::npos);
 std::ofstream(outputDirectory+"/config.json")<<server.body;
 server.routes["/api/connection"]();std::ofstream(outputDirectory+"/connection.json")<<server.body;
 server.routes["/api/status"]();std::ofstream(outputDirectory+"/status.json")<<server.body;
 assert(jsonString("a\"b\\c\n") == "\"a\\\"b\\\\c\\u000a\"");
 server.auth=false;server.routes["/api/status"]();assert(server.code==401);server.auth=true;
 server.args={{"csrf","wrong"}};server.routes["/api/schedule"]();assert(server.code==403&&server.body.find("error")!=String::npos);
 scheduleArgs();server.args["nightAmbient"]="nan";server.routes["/api/schedule"]();assert(server.code==422&&server.body.find("error")!=String::npos);
 scheduleArgs();server.routes["/api/schedule"]();assert(server.code==200&&server.body.find("\"ok\":true")!=String::npos);
 server.args={{"view","monitor"}};render();assert(server.body.find("location.reload")==String::npos&&server.body.find("/api/status")!=String::npos);

 // Existing protection stays enabled when saved without a replacement password.
 server.args={{"csrf",csrf},{"protectAdmin","1"}};
 server.routes["/api/admin"]();assert(server.code==200&&String(deviceConfig.adminPassword)=="dashboard123"&&server.body.find("\"reauthenticate\":false")!=String::npos);
 server.auth=false;server.args={{"csrf",csrf}};server.routes["/api/admin"]();assert(server.code==401&&deviceConfig.adminPassword[0]);server.auth=true;
 server.args={{"csrf","wrong"}};server.routes["/api/admin"]();assert(server.code==403&&deviceConfig.adminPassword[0]);
 saveSucceeds=false;server.args={{"csrf",csrf}};server.routes["/api/admin"]();assert(server.code==500&&deviceConfig.adminPassword[0]);saveSucceeds=true;
 server.routes["/api/admin"]();assert(server.code==200&&!deviceConfig.adminPassword[0]);
 server.auth=false;server.routes["/"]();assert(server.code==200);server.routes["/api/status"]();assert(server.code==200);
 server.routes["/api/config"]();assert(server.body.find("\"adminProtected\":false")!=String::npos);
 server.args={{"csrf","wrong"},{"protectAdmin","1"},{"adminPassword","new-password"}};server.routes["/api/admin"]();assert(server.code==403&&!deviceConfig.adminPassword[0]);
 server.args={{"csrf",csrf},{"protectAdmin","1"}};server.routes["/api/admin"]();assert(server.code==422&&!deviceConfig.adminPassword[0]);
 server.args["adminPassword"]="short";server.routes["/api/admin"]();assert(server.code==422&&!deviceConfig.adminPassword[0]);
 server.args["adminPassword"]="new-password";server.routes["/api/admin"]();assert(server.code==200&&String(deviceConfig.adminPassword)=="new-password"&&server.body.find("\"reauthenticate\":true")!=String::npos);
 server.routes["/api/status"]();assert(server.code==401);server.auth=true;
 server.args={{"csrf",csrf},{"ssid","Other Wi-Fi"},{"wifiPassword",""}};server.routes["/api/wifi"]();assert(server.code==200&&String(deviceConfig.adminPassword)=="new-password");
 // First-time setup can explicitly omit protection.
 deviceConfig=DeviceConfig{};server.auth=false;
 server.args={{"csrf",csrf},{"ssid","Open dashboard"},{"wifiPassword",""}};
 server.routes["/api/wifi"]();assert(server.code==200&&!deviceConfig.adminPassword[0]);server.routes["/"]();assert(server.code==200);
 server.args={{"csrf",csrf},{"confirmation","no"}};server.routes["/reset"]();assert(server.code==422&&!resetRequested);
 server.args={{"csrf",csrf},{"confirmation","RESET"}};server.routes["/api/reset"]();assert(server.code==200&&resetRequested);
 std::cout<<"Optional password setup, protection toggles and actual firmware pages rendered; authentication, CSRF, validation, saving, probe mapping, escaping and reset confirmation checks passed with mocked hardware.\n";
}
