#include "../DeviceState.cpp"
#include "../DeviceWeb.cpp"
#include <cassert>
bool historyStorageHealthy=true;
void serviceTemperatureHistory(){}
bool eraseTemperatureHistory(){return true;}
int main(){
 stateMutex=1;csrf="token";
 auto start=[] {
   server.requestHeaders={{"X-CSRF-Token",csrf},{"X-Firmware-Size","4"}};
   server.currentUpload=HTTPUpload{};receiveFirmware();
 };
 auto write=[] {server.currentUpload.status=UPLOAD_FILE_WRITE;server.currentUpload.currentSize=4;server.currentUpload.buf[0]=0xe9;receiveFirmware();};
 auto end=[] {server.currentUpload.status=UPLOAD_FILE_END;receiveFirmware();finishFirmware();};
 Update.onFlash=[] {assert(maintenance&&firmwareHolding);};
 // Missing credentials/CSRF must not begin writing flash.
 strcpy(deviceConfig.adminPassword,"password");server.auth=false;start();write();end();assert(server.code==401&&Update.begins==0&&!maintenance);
 server.auth=true;server.requestHeaders={{"X-CSRF-Token","bad"}};receiveFirmware();finishFirmware();assert(server.code==403&&Update.begins==0);
 clearFirmwareRequest();fakePartitionPresent=false;start();write();end();assert(server.code==422&&Update.begins==0);fakePartitionPresent=true;
 for (const char* size : {"", "0", "-4", "4junk", "99999999", "100000000"}) {
   server.requestHeaders={{"X-CSRF-Token",csrf},{"X-Firmware-Size",size}};
   server.currentUpload=HTTPUpload{};receiveFirmware();write();end();assert(server.code==422&&Update.begins==0);
 }
 for (const char* filename : {"app.txt", "app.merged.bin", "app.bootloader.bin", "app.partitions.bin"}) {
   server.requestHeaders={{"X-CSRF-Token",csrf},{"X-Firmware-Size","4"}};
   server.currentUpload=HTTPUpload{};server.currentUpload.filename=filename;
   receiveFirmware();write();end();assert(server.code==422&&Update.begins==0);
 }
 // No flash write if the actual controller has not acknowledged outputs off.
 controllerRunning=true;start();write();end();assert(server.code==503&&Update.begins==0&&!maintenance);controllerRunning=false;
 Update.beginOK=false;start();write();end();assert(server.code==500&&!maintenance);Update.beginOK=true;
 // Truncation, invalid magic, flash errors, extra files and failed validation.
 start();end();assert(server.code==422&&!maintenance&&Update.ends==0);
 start();server.currentUpload.status=UPLOAD_FILE_WRITE;server.currentUpload.currentSize=4;receiveFirmware();end();assert(server.code==422&&!maintenance);
 Update.writeOK=false;start();write();end();assert(server.code==422&&!maintenance);Update.writeOK=true;
 start();write();server.currentUpload.status=UPLOAD_FILE_START;receiveFirmware();end();assert(server.code==422&&!maintenance);
 Update.endOK=false;start();write();end();assert(server.code==422&&!maintenance&&!firmwareRebootPending);Update.endOK=true;
 start();write();server.currentUpload.status=UPLOAD_FILE_ABORTED;receiveFirmware();assert(!maintenance&&!Update.running);
 start();write();testMillis+=120001;write();end();assert(server.code==408&&!maintenance);
 start();write();write();end();assert(server.code==422&&!maintenance);
 start();write();clearFirmwareRequest();assert(!maintenance&&!Update.running); // Parser failure cleanup.
 // Successful image holds outputs off through reboot and preserves configuration.
 const DeviceConfig before=deviceConfig;
 start();write();end();assert(server.code==200&&firmwareRebootPending&&maintenance);
 clearFirmwareRequest();assert(maintenance&&firmwareHolding);
 assert(!memcmp(&before,&deviceConfig,sizeof(before))&&!resetRequested);
 std::cout<<"Firmware authorization, CSRF, partition checks, output-off timeout, upload failures, validation and reboot guard passed.\n";
}
