#pragma once
// Included inside DeviceWeb's namespace, after its authentication/JSON helpers.
// Multipart callbacks run on the web task. Never write flash until the controller
// acknowledges both outputs off; keep the guard held through validation/reboot.
bool firmwareHolding = false, firmwareSeen = false, firmwareComplete = false;
bool firmwareRebootPending = false;
uint32_t firmwareRebootAt = 0, firmwareStarted = 0;
size_t firmwareExpected = 0, firmwareReceived = 0;
String firmwareError;
int firmwareErrorCode = 422;

void failFirmware(const char* message, int code = 422) {
  if (firmwareError.isEmpty()) { firmwareError = message; firmwareErrorCode = code; }
  if (firmwareHolding) Update.abort();
}
void clearFirmwareRequest() {
  if (firmwareRebootPending) return;
  if (firmwareHolding) { Update.abort(); endFlashMaintenance(); }
  firmwareHolding = firmwareSeen = firmwareComplete = false;
  firmwareExpected = firmwareReceived = 0;
  firmwareError = ""; firmwareErrorCode = 422;
}
bool firmwareCredentials() {
  const DeviceConfig config = configSnapshot();
  return !config.adminPassword[0] || server.authenticate("admin", config.adminPassword);
}
void receiveFirmware() {
  HTTPUpload& upload = server.upload();
  if (upload.status == UPLOAD_FILE_START) {
    if (firmwareSeen) { failFirmware("Upload exactly one application .bin file."); return; }
    firmwareSeen = true;
    if (!firmwareCredentials()) { failFirmware("Please sign in again.", 401); return; }
    if (server.header("X-CSRF-Token") != csrf || csrf.isEmpty()) {
      failFirmware("This form has expired. Reload the Device page and retry.", 403); return;
    }
    if (resetRequested) { failFirmware("The device is resetting.", 409); return; }
    const String size = server.header("X-Firmware-Size");
    // Bound the decimal parse before conversion; the default app slot is 1.25 MiB.
    if (size.isEmpty() || size.length() > 8) { failFirmware("Missing or invalid firmware size."); return; }
    for (size_t i=0; i<size.length(); ++i)
      if (size[i] < '0' || size[i] > '9') { failFirmware("Invalid firmware size."); return; }
    firmwareExpected = strtoul(size.c_str(), nullptr, 10);
    const esp_partition_t* next = esp_ota_get_next_update_partition(nullptr);
    if (!next || !firmwareExpected || firmwareExpected > next->size) {
      failFirmware("Firmware does not fit the OTA application partition. Check the board and build settings."); return;
    }
    if (upload.name != "firmware" || !upload.filename.endsWith(".bin") ||
        upload.filename.endsWith(".merged.bin") || upload.filename.endsWith(".bootloader.bin") ||
        upload.filename.endsWith(".partitions.bin")) {
      failFirmware("Choose the application .bin file, not a merged or filesystem image."); return;
    }
    if (!beginFlashMaintenance()) { failFirmware("Could not confirm lamps off. Retry the update.", 503); return; }
    firmwareHolding = true;
    firmwareStarted = millis();
    if (!Update.begin(firmwareExpected, U_FLASH)) failFirmware("Could not start firmware update. Check the OTA partition.", 500);
  } else if (upload.status == UPLOAD_FILE_WRITE && firmwareHolding && firmwareError.isEmpty()) {
    if (uint32_t(millis()-firmwareStarted) > 120000) { failFirmware("Upload timed out. Retry with a stable Wi-Fi connection.", 408); return; }
    if (upload.currentSize > firmwareExpected-firmwareReceived) { failFirmware("Firmware size does not match the selected file."); return; }
    if (!firmwareReceived && (!upload.currentSize || upload.buf[0] != 0xe9)) {
      failFirmware("Invalid ESP32 application image."); return;
    }
    if (Update.write(upload.buf, upload.currentSize) != upload.currentSize) {
      failFirmware("Firmware write failed. Check the application image and retry."); return;
    }
    firmwareReceived += upload.currentSize;
  } else if (upload.status == UPLOAD_FILE_END) {
    firmwareComplete = firmwareHolding && firmwareError.isEmpty() && firmwareReceived == firmwareExpected;
    if (!firmwareComplete && firmwareError.isEmpty()) failFirmware("Firmware upload was incomplete.");
  } else if (upload.status == UPLOAD_FILE_ABORTED) {
    failFirmware("Firmware upload was interrupted.");
    clearFirmwareRequest(); // The parser may return without calling the completion handler.
  }
}
void finishFirmware() {
  if (!authorized()) { clearFirmwareRequest(); return; }
  if (server.header("X-CSRF-Token") != csrf || csrf.isEmpty()) {
    jsonError("This form has expired. Reload the Device page and retry.", 403);
    clearFirmwareRequest(); return;
  }
  if (firmwareError.isEmpty() && (!firmwareComplete || !firmwareHolding)) failFirmware("Choose an application .bin file.");
  if (firmwareHolding && uint32_t(millis()-firmwareStarted) > 120000) failFirmware("Upload timed out. Retry with a stable Wi-Fi connection.", 408);
  if (firmwareError.isEmpty() && !Update.end()) failFirmware("Firmware validation failed. The current firmware is still installed.");
  if (!firmwareError.isEmpty()) {
    jsonError(firmwareError, firmwareErrorCode); clearFirmwareRequest(); return;
  }
  firmwareRebootPending = true;
  firmwareRebootAt = millis()+1000;
  sendJson("{\"ok\":true,\"restarting\":true}");
}
