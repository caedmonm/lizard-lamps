#include "../DeviceWeb.cpp"
#include <fstream>
#include <cassert>
DeviceConfig deviceConfig;
DeviceStatus deviceStatus;
LampTest lampTest;
TemperatureHistory temperatureHistory;
bool historyStorageHealthy=true;
void serviceTemperatureHistory(){}
bool eraseTemperatureHistory(){temperatureHistory.clear();return true;}
SemaphoreHandle_t stateMutex=1;
bool maintenance=false,maintenanceAcknowledged=false,controllerRunning=false,resetRequested=false;
DeviceConfig configSnapshot(){return deviceConfig;}
DeviceStatus statusSnapshot(){return deviceStatus;}
bool saveSucceeds = true;
bool saveConfig(const DeviceConfig& c){if (!saveSucceeds) return false;deviceConfig=c;return true;}
bool loadConfig(){return true;}
bool beginFlashMaintenance(bool){maintenance=true;return true;}
void endFlashMaintenance(){maintenance=false;}
int main(int argc, char** argv){
 assert(DeviceConfig{}.fadeSeconds==1.0f);
 const std::string outputDirectory = argc > 1 ? argv[1] : "/tmp";
 try{webTask(nullptr);}catch(const std::runtime_error&){}
 assert(WiFi.openAP);
 assert(server.routes.count("/api/firmware")&&server.uploads.count("/api/firmware"));
 server.auth=false;server.routes["/favicon.svg"]();
 assert(server.code==200&&server.contentType=="image/svg+xml"&&server.body.find("<svg")!=String::npos);
 server.auth=true;
 strcpy(deviceConfig.adminPassword,"password");
 server.auth=false;server.routes["/api/history"]();assert(server.code==401);
 server.auth=true;deviceConfig.adminPassword[0]=0;
 server.routes["/api/history"]();assert(server.body.find("\"samples\":[]")!=String::npos);
 const uint32_t historyNow=uint32_t(time(nullptr));
 const float historyTargets[2][2]={{30,24},{20,18}};const int historyPower[2]={60,0};
 temperatureHistory.record(historyNow-historyNow%60-60,30,24,true,historyTargets,historyPower);
 temperatureHistory.record(historyNow,-127,46,true,historyTargets,historyPower);
 server.routes["/api/history"]();assert(server.code==200);
 std::ofstream(outputDirectory+"/history.json")<<server.body;
 assert(server.headers["Cache-Control"]=="no-store");
 temperatureHistory.clear();
 for(unsigned i=0;i<TemperatureHistory::Capacity;++i)
   temperatureHistory.record(historyNow-historyNow%60-(TemperatureHistory::Capacity-1-i)*60,29,23,true,historyTargets,historyPower);
 server.routes["/api/history"]();
 std::ofstream(outputDirectory+"/history-full.json")<<server.body;
 // Fetch the same complete day in bounded pages, newest page first. Epoch
 // cursors remain valid when indices move as new readings arrive or expire.
 server.args={{"paged","1"}};
 std::ofstream pages(outputDirectory+"/history-pages.json");pages<<"[";
 uint32_t before=0;unsigned pageCount=0;
 do {
   server.routes["/api/history"]();assert(server.code==200);
   if(pageCount++)pages<<",";pages<<server.body;
   const size_t cursor=server.body.find("\"nextBefore\":")+13;
   const uint32_t next=std::stoul(server.body.substr(cursor));
   assert(!next||!before||next<before);before=next;
   server.args["before"]=std::to_string(before);
 }while(before);
 pages<<"]";pages.close();assert(pageCount>=12&&pageCount<=13);
 for(const char* invalid : {"", "bad", "1704067200junk", "-1", "4102444800", "99999999999999999999"}) {
   server.args["before"]=invalid;server.routes["/api/history"]();assert(server.code==422);
 }
 server.args={{"paged","1"},{"before",std::to_string(temperatureHistory.at(0).epoch)}};
 server.routes["/api/history"]();assert(server.body.find("\"samples\":[]")!=String::npos);
 assert(server.body.find("\"nextBefore\":0")!=String::npos);
 server.args.clear();
 temperatureHistory.clear();
 csrf="test-token"; hotspot=false;
 // Manual test writes require authentication, CSRF and healthy, fresh hardware.
 auto testArgs=[&] {server.args={{"csrf",csrf},{"enabled","1"},{"lamp","0"},{"percent","37"}};};
 strcpy(deviceConfig.adminPassword,"password");
 testArgs();server.auth=false;server.routes["/api/test"]();assert(server.code==401&&!lampTest.active);server.auth=true;
 testArgs();server.args["csrf"]="wrong";server.routes["/api/test"]();assert(server.code==403&&!lampTest.active);
 deviceConfig.adminPassword[0]=0;
 for(const char* bad:{"", "-1", "101", "37.5", "nan", "10x", " 5", "1000"}) {
   testArgs();server.args["percent"]=bad;server.routes["/api/test"]();assert(server.code==422&&!lampTest.active);
 }
 testArgs();server.args["lamp"]="2";server.routes["/api/test"]();assert(server.code==422);
 testArgs();server.args["enabled"]="yes";server.routes["/api/test"]();assert(server.code==422);
 testArgs();server.routes["/api/test"]();assert(server.code==409&&!lampTest.active);
 deviceStatus.sensorsHealthy=deviceStatus.dimmerOnline=true;deviceStatus.lastReading=testMillis;
 testArgs();server.routes["/api/test"]();assert(server.code==200&&lampTest.active&&lampTest.percent==37&&lampTest.lamp==0);
 testArgs();server.args["lamp"]="1";server.args["percent"]="100";server.routes["/api/test"]();assert(lampTest.percent==100&&lampTest.lamp==1);
 testArgs();server.args["percent"]="0";server.routes["/api/test"]();assert(server.code==200&&lampTest.active&&lampTest.percent==0);
 deviceStatus.lastReading=testMillis-4000;
 testArgs();server.routes["/api/test"]();assert(server.code==409);
 deviceStatus.sensorsHealthy=false;
 server.args={{"csrf",csrf},{"enabled","0"}};server.routes["/api/test"]();assert(server.code==200&&!lampTest.active);
 deviceStatus=DeviceStatus{};

 server.routes["/api/status"]();assert(server.body.find("null")!=String::npos);assert(server.contentType.find("application/json")!=String::npos);
 std::ofstream(outputDirectory+"/status-empty.json")<<server.body;

 render(); std::ofstream(outputDirectory + "/setup.html")<<server.body;
 assert(server.body.find("Choose a dashboard password")!=String::npos);
 assert(server.body.find("href='/favicon.svg'")!=String::npos);
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
 auto controlArgs = [&] { server.args={{"csrf",csrf},{"dimmingEnabled","0"},{"fadeSeconds","4"},{"proportionalBand","2"},{"integralSeconds","300"}}; };
 server.auth=false;controlArgs();server.routes["/api/control"]();assert(server.code==401&&deviceConfig.dimmingEnabled);server.auth=true;
 controlArgs();server.args["csrf"]="wrong";server.routes["/api/control"]();assert(server.code==403&&deviceConfig.dimmingEnabled);
 for(const char* field:{"dimmingEnabled","proportionalBand","integralSeconds"}){
   controlArgs();server.args.erase(field);server.routes["/api/control"]();assert(server.code==422&&deviceConfig.dimmingEnabled);
 }
 for(const char* bad:{"nan","inf","0","-2","10.1","2junk"}){
   controlArgs();server.args["proportionalBand"]=bad;server.routes["/api/control"]();assert(server.code==422&&deviceConfig.dimmingEnabled);
   controlArgs();server.args["fadeSeconds"]=String(bad)=="10.1"?"300.1":bad;server.routes["/api/control"]();assert(server.code==422&&deviceConfig.dimmingEnabled);
 }
 for(const char* bad:{"nan","inf","0","59","3601","300junk",""}){
   controlArgs();server.args["integralSeconds"]=bad;server.routes["/api/control"]();assert(server.code==422);
 }
 controlArgs();server.args["proportionalBand"]="0.9";server.routes["/api/control"]();assert(server.code==422);
 controlArgs();server.args["dimmingEnabled"]="1";server.args.erase("fadeSeconds");server.routes["/api/control"]();assert(server.code==422&&deviceConfig.dimmingEnabled);
 controlArgs();server.args["dimmingEnabled"]="yes";server.routes["/api/control"]();assert(server.code==422&&deviceConfig.dimmingEnabled);
 saveSucceeds=false;controlArgs();server.routes["/api/control"]();assert(server.code==500&&deviceConfig.dimmingEnabled);saveSucceeds=true;
 controlArgs();server.routes["/api/control"]();assert(server.code==200&&!deviceConfig.dimmingEnabled&&deviceConfig.fadeSeconds==4&&deviceConfig.proportionalBand==2);
 controlArgs();server.args.erase("fadeSeconds");server.args["proportionalBand"]="1.5";server.routes["/api/control"]();assert(server.code==200&&deviceConfig.fadeSeconds==4&&deviceConfig.proportionalBand==1.5f);
 controlArgs();server.routes["/api/control"]();assert(server.code==200&&deviceConfig.proportionalBand==2);
 assert(deviceConfig.targets[0][0]==30&&deviceConfig.starts[0]==420);
 server.args={{"view","device"}};render();assert(server.body.find("Firmware update")!=String::npos&&server.body.find("firmware-form")!=String::npos);
 assert(server.body.find("<button disabled>Upload firmware &amp; restart</button>")!=String::npos);
 assert(server.body.find("method='post' action='/api/firmware'")!=String::npos);
 assert(server.body.find("Loading firmware uploader")!=String::npos);
 server.args={{"view","settings"}};render();assert(server.body.find("Holding adjustment time")!=String::npos);assert(server.body.find("Proportional band")!=String::npos&&server.body.find("value='0' selected")!=String::npos);
 controlArgs();server.args["dimmingEnabled"]="1";server.routes["/control"]();assert(server.code==303&&deviceConfig.dimmingEnabled);
 server.routes["/api/config"]();assert(server.body.find("\"dimmingEnabled\":true")!=String::npos&&server.body.find("\"fadeSeconds\":4.0")!=String::npos&&server.body.find("\"proportionalBand\":2.0")!=String::npos);
 controlArgs();server.args["minimumTemperature"]="17.5";server.routes["/api/control"]();
 assert(server.code==200&&deviceConfig.minimumTemperature==17.5f);
 for(const char* bad:{"nan","inf","4.9","40.1","17junk",""}){
   controlArgs();server.args["minimumTemperature"]=bad;server.routes["/api/control"]();
   assert(server.code==422&&deviceConfig.minimumTemperature==17.5f);
 }
 controlArgs();server.routes["/api/control"]();assert(deviceConfig.minimumTemperature==17.5f);
 server.routes["/api/config"]();assert(server.body.find("\"minimumTemperature\":17.5")!=String::npos);
 for(const char* fade:{"0.1","1","300"}) {
   controlArgs();server.args["dimmingEnabled"]="1";server.args["fadeSeconds"]=fade;
   server.routes["/api/control"]();assert(server.code==200&&deviceConfig.fadeSeconds==std::stof(fade));
 }
 controlArgs();server.args["fadeSeconds"]="4";server.routes["/api/control"]();
 deviceStatus.floorMode=Control::FloorMode::Recovery;deviceStatus.activeLamp=2;
 server.args={{"view","monitor"}};render();assert(server.body.find("Minimum recovery · Both lamps")!=String::npos);
 assert(server.body.find("id='history-chart'")!=String::npos && server.body.find("value='24'")!=String::npos && server.body.find("value='48'")==String::npos);
 server.routes["/api/status"]();assert(server.body.find("\"floorMode\":1")!=String::npos&&server.body.find("\"lampPower\":[")!=String::npos);
 deviceStatus.targetLimited=true;
 server.routes["/api/status"]();assert(server.body.find("\"targetLimited\":true")!=String::npos);
 deviceStatus.targetLimited=false;
 deviceStatus.floorMode=Control::FloorMode::NightHold;
 server.args={{"view","monitor"}};render();assert(server.body.find("Minimum hold · Night lamp")!=String::npos);
 deviceStatus.floorMode=Control::FloorMode::Normal;
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
 // Serve the actual full-history route through the guarded server. Failed
 // sockets deliberately remain "connected" until the application closes them.
 temperatureHistory.clear();
 for(unsigned i=0;i<TemperatureHistory::Capacity;++i)
   temperatureHistory.record(historyNow-historyNow%60-(TemperatureHistory::Capacity-1-i)*60,29,23,true,historyTargets,historyPower);
 server.request=[&]{server.routes["/api/history"]();};
 for(unsigned failAt : {1U, 6U}) { // Response header, then a streamed payload.
   server.client()=NetworkClient{};server.client().failAt=failAt;
   server.handleClient();
   assert(server.responseFailed()&&!server.client().connected());
   assert(server.client().writes==failAt); // No retries for the remaining 24 hours.
 }
 server.client()=NetworkClient{};server.client().writeLimit=3;
 server.handleClient();
 assert(server.responseFailed()&&!server.client().connected()&&server.client().writes==1);
 // Successful but slow writes hit the inter-write response budget, including rollover.
 for(uint32_t start : {100U, UINT32_MAX-4000}) {
   testMillis=start;server.client()=NetworkClient{};server.client().writeDelay=1000;
   server.handleClient();
   assert(server.responseFailed()&&!server.client().connected());
   assert(uint32_t(testMillis-start)>=8000&&uint32_t(testMillis-start)<=9000);
 }
 // A slow library write may overrun the budget once, then the response is abandoned.
 server.client()=NetworkClient{};server.client().writeDelay=10000;
 server.handleClient();assert(server.responseFailed()&&server.client().writes==1);
 // The next client gets a complete response; failure state is per request.
 server.client()=NetworkClient{};server.handleClient();
 assert(!server.responseFailed()&&server.body.find("\"samples\":[[")!=String::npos);
 assert(server.body.substr(server.body.size()-2)=="]}");
 // At 100ms per socket write the full day exceeds the response budget, but
 // a two-hour page completes and releases the web task for the next request.
 server.client()=NetworkClient{};server.client().writeDelay=100;server.handleClient();
 assert(server.responseFailed());
 server.args={{"paged","1"}};
 server.client()=NetworkClient{};server.client().writeDelay=100;server.handleClient();
 assert(!server.responseFailed()&&server.body.substr(server.body.size()-2)=="]}");
 server.args.clear();
 server.request=[&]{server.routes["/api/status"]();};
 server.client()=NetworkClient{};server.handleClient();
 assert(server.code==200&&!server.responseFailed()&&server.body.find("\"temperatures\":[")!=String::npos);
 // Pages stream each script from flash after their HTML, with an exact total
 // length. A failed part must not be followed by more script writes.
 server.request=[&]{server.args={{"view","device"}};render();};
 for(unsigned failAt : {1U,2U,3U,4U,5U,6U}) {
   server.client()=NetworkClient{};server.client().failAt=failAt;
   server.handleClient();assert(server.responseFailed()&&server.client().writes==failAt);
 }
 server.client()=NetworkClient{};server.handleClient();
 assert(!server.responseFailed()&&!server.chunked&&server.contentLength==server.body.size());
 assert(server.body.find(WebClient)!=String::npos&&server.body.find(WebFirmware)!=String::npos&&server.body.find(WebHistory)!=String::npos);
 assert(server.body.substr(server.body.size()-14)=="</body></html>");
 std::cout<<"Optional password setup, protection toggles and actual firmware pages rendered; authentication, CSRF, validation, saving, probe mapping, escaping and reset confirmation checks passed with mocked hardware.\n";
 std::cout<<"HTTP short/failed writes, slow-response budget, millis rollover and next-client recovery passed with mocked sockets.\n";
}
