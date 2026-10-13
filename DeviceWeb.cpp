#include "DeviceState.h"
#include "ControlLogic.h"
#include "WebStyle.h"
#include "WebIcon.h"
#include "WebClient.h"
#include <WiFi.h>
#include <ESPmDNS.h>
#include <WebServer.h>
#include <DNSServer.h>
#include <Preferences.h>
#include <esp_system.h>
#include <sys/time.h>
#include <time.h>

namespace {
WebServer server(80);
DNSServer dns;
String hotspotName, csrf;
bool mdnsStarted = false;
bool apiRequest = false;
bool hotspot = false;
bool reconnectRequested = false;
unsigned long disconnectedSince = 0, connectedSince = 0, rebootAt = 0;
bool wasConnected = false;
const char* Zone = "GMT0BST,M3.5.0/1,M10.5.0/2";

String escape(const String& value) {
  String out;
  out.reserve(value.length() + 16);
  for (size_t i = 0; i < value.length(); ++i) {
    switch(value[i]) {
      case '&': out += "&amp;"; break; case '<': out += "&lt;"; break;
      case '>': out += "&gt;"; break; case '"': out += "&quot;"; break;
      case '\'': out += "&#39;"; break; default: out += value[i];
    }
  }
  return out;
}
String jsonString(const String& value) {
  String result = "\"";
  for (size_t i = 0; i < value.length(); ++i) {
    unsigned char c = value[i];
    if (c == '"' || c == '\\') { result += '\\'; result += char(c); }
    else if (c < 32) { char escaped[7]; snprintf(escaped, sizeof(escaped), "\\u%04x", c); result += escaped; }
    else result += char(c);
  }
  return result + "\"";
}
String jsonNumber(float value) { return isfinite(value) && value != -127.0f ? String(value, 2) : String("null"); }
const char* jsonBool(bool value) { return value ? "true" : "false"; }
void sendJson(const String& body, int code = 200) {
  server.sendHeader("Cache-Control", "no-store");
  server.sendHeader("X-Content-Type-Options", "nosniff");
  server.send(code, "application/json; charset=utf-8", body);
}
void jsonError(const String& error, int code) { sendJson("{\"error\":" + jsonString(error) + "}", code); }
void registerForm(const char* path, const std::function<void()>& handler) {
  server.on(path, HTTP_POST, handler);
  String apiPath = String("/api") + path;
  server.on(apiPath.c_str(), HTTP_POST, [handler] { apiRequest = true; handler(); apiRequest = false; });
}
String timeText(int minute) {
  char buf[6]; snprintf(buf, sizeof(buf), "%02d:%02d", minute / 60, minute % 60); return buf;
}
String clockText() {
  time_t epoch = time(nullptr); tm local = {};
  if (epoch < 1704067200) return "Waiting for clock";
  localtime_r(&epoch, &local); char buf[48]; strftime(buf, sizeof(buf), "%a %d %b · %H:%M %Z", &local); return buf;
}
String address(const uint8_t* probe) {
  char buf[17]; for (int i = 0; i < 8; ++i) snprintf(buf + i * 2, 3, "%02X", probe[i]); return buf;
}
String hidden() {
  return "<input type='hidden' name='csrf' value='" + csrf + "'><input type='hidden' name='epoch' class='epoch'>";
}
String formStart(const char* route) { return String("<form method='post' action='") + route + "'>" + hidden(); }
String brand() {
  return "<div class='brand'><svg width='30' height='30' viewBox='0 0 32 32' fill='none' aria-hidden='true'><path d='M16 3v5M6 8l4 4M3 18h5M26 8l-4 4M29 18h-5M10 24h12M12 28h8' stroke='currentColor' stroke-width='2' stroke-linecap='round'/><path d='M10 20a8 8 0 1 1 12 0v4H10z' stroke='currentColor' stroke-width='2'/></svg>Lizard Lamps</div>";
}
bool authorized() {
  DeviceConfig c = configSnapshot();
  if (!c.adminPassword[0]) return true;
  if (server.authenticate("admin", c.adminPassword)) return true;
  server.requestAuthentication(DIGEST_AUTH, "Lizard Lamps", "Sign in with admin and your dashboard password.");
  return false;
}
bool validPost() {
  if (!authorized()) return false;
  if (!server.hasArg("csrf") || server.arg("csrf") != csrf) {
    if (apiRequest) jsonError("This form has expired. Open the dashboard again and retry.", 403);
    else server.send(403, "text/plain", "This form has expired. Reload the page and try again."); return false;
  }
  return true;
}
void sendPage(String html, int code = 200) {
  server.sendHeader("Cache-Control", "no-store");
  server.sendHeader("X-Content-Type-Options", "nosniff");
  server.sendHeader("X-Frame-Options", "DENY");
  server.sendHeader("Content-Security-Policy", "default-src 'self'; style-src 'self' 'unsafe-inline'; script-src 'self' 'unsafe-inline'; base-uri 'none'; frame-ancestors 'none'; form-action 'self'");
  server.send(code, "text/html; charset=utf-8", html);
}
String beginning() {
  String html; html.reserve(16000);
  html = "<!doctype html><html lang='en'><head><meta charset='utf-8'><meta name='viewport' content='width=device-width,initial-scale=1'><title>Lizard Lamps</title><link rel='icon' type='image/svg+xml' href='/favicon.svg'>";
  html += FPSTR(WebStyle); html += "</head><body>"; return html;
}
String ending() { return String(FPSTR(WebClient)) + "</body></html>"; }
void redirect(const String& path) {
  if (apiRequest) { sendJson("{\"ok\":true,\"next\":" + jsonString(path) + "}"); return; }
  server.sendHeader("Location", path); server.send(303, "text/plain", "Saved");
}
void bootstrapClock() {
  // Browser time is a fallback only while the clock is unset; NTP corrects it later.
  if (time(nullptr) >= 1704067200 || !server.hasArg("epoch")) return;
  String text = server.arg("epoch"); char* end;
  long long epoch = strtoll(text.c_str(), &end, 10);
  if (*end || epoch < 1704067200 || epoch >= 4102444800LL) return;
  timeval tv = {static_cast<time_t>(epoch), 0}; settimeofday(&tv, nullptr);
}
String adminFields(bool initial, const DeviceConfig& config) {
  bool enabled = initial || config.adminPassword[0];
  String h = "<label class='gap'><input type='checkbox' name='protectAdmin' value='1' data-admin-toggle" + String(enabled ? " checked" : "") + "> Require a password to manage this device</label>";
  h += "<label>" + String(initial ? "Choose a dashboard password" : "Dashboard password") + "<input type='password' name='adminPassword' data-admin-password minlength='8' maxlength='63' autocomplete='new-password'></label>";
  h += "<p class='muted small'>When enabled, sign in as <strong>admin</strong>. Use 8–63 characters." + String(initial ? "" : " Leave the password blank to keep your current password when protection is already on.") + " When off, anyone connected to the network can manage this device.</p>";
  return h;
}
String wifiForm(bool initial, const DeviceConfig& c) {
  String h = formStart("/wifi");
  if (initial) h += "<input type='hidden' name='adminCurrent' value='0' data-admin-current>";
  h += "<label for='ssid'>Wi-Fi network name</label><input id='ssid' name='ssid' autocomplete='off' required maxlength='32' value='" + escape(c.ssid) + "'>";
  h += "<div class='gap'><label for='wifiPassword'>Wi-Fi password</label><input id='wifiPassword' name='wifiPassword' type='password' maxlength='63' autocomplete='new-password'><p class='muted small'>Enter the network password. Leave empty only for an open network. 2.4GHz Wi-Fi is required.</p></div>";
  if (initial) h += adminFields(true, c);
  h += "<button type='submit'>"; h += initial ? "Connect & continue →" : "Save Wi-Fi details"; h += "</button></form>";
  return h;
}
String timeline(const DeviceConfig& c) {
  String h = "<div class='timeline' role='img' aria-label='Daily lamp schedule: lamp 1 starts at " + timeText(c.starts[0]) + ", lamp 2 starts at " + timeText(c.starts[1]) + "'>";
  auto segment = [&](int start, int end) {
    h += "<span style='left:" + String(start / 14.4f, 3) + "%;width:" + String((end - start) / 14.4f, 3) + "%'></span>";
  };
  if (c.starts[0] < c.starts[1]) segment(c.starts[0], c.starts[1]);
  else { segment(0, c.starts[1]); segment(c.starts[0], 1440); }
  return h + "</div><div class='ticks'><span>00:00</span><span>06:00</span><span>12:00</span><span>18:00</span><span>24:00</span></div><div class='legend'><span><i class='swatch'></i>Day · Lamp 1</span><span><i class='swatch purple'></i>Night · Lamp 2</span></div>";
}
void render(const String& error = "", int code = 200, const String& forcedView = "") {
  if (apiRequest) { jsonError(error, code); return; }
  DeviceConfig c = configSnapshot(); DeviceStatus s = statusSnapshot();
  String h = beginning();
  if (!c.ssid[0]) {
    h += "<div class='setup-shell'><div class='welcome'>" + brand() + "<div class='card'><span class='eyebrow'>Welcome home</span><h1>A little warmth.<br>A smarter habitat.</h1><div id='live-message' class='notice' role='status' hidden></div><p class='muted'>Connect your controller to Wi-Fi to start setting up your enclosure.</p>";
    if (error.length()) h += "<div class='notice error' role='alert'>" + escape(error) + "</div>";
    h += wifiForm(true, c) + "</div><p class='footer'>Join LizardLamps-XXXXXX without a password, then open <a href='http://192.168.4.1'>http://192.168.4.1</a>. Heating stays off during setup.</p></div></div>" + ending(); sendPage(h, code); return;
  }
  String view = forcedView.isEmpty() ? server.arg("view") : forcedView;
  if (view != "settings" && view != "schedule" && view != "device") view = c.targetsSet && c.scheduleSet ? "monitor" : "schedule";
  h += "<div class='shell'><aside>" + brand() + "<div><div class='tag'>Your habitat</div><nav class='nav' aria-label='Main navigation'>";
  const char* views[] = {"monitor", "schedule", "settings", "device"};
  const char* labels[] = {"Overview", "Schedule & temps", "Settings", "Device"};
  for (int i = 0; i < 4; ++i) h += "<a class='" + String(view == views[i] ? "active" : "") + "' href='/?view=" + views[i] + "'>" + labels[i] + "</a>";
  h += "</nav></div><div class='sidefoot'><span class='dot'></span>On-device control<br>Europe/London · 50Hz</div></aside><main><div id='live-message' class='notice' role='status' hidden></div>";
  if (error.length()) h += "<div class='notice error' role='alert'>" + escape(error) + "</div>";
  if (server.hasArg("saved")) h += "<div class='notice success' role='status'>Your changes have been saved.</div>";
  if (hotspot) {
    if (WiFi.status() == WL_CONNECTED) h += "<div class='notice success'>Connected to Wi-Fi. Rejoin your usual network and open <a href='http://" + WiFi.localIP().toString() + "'>" + WiFi.localIP().toString() + "</a>. You can also use <a href='http://lizard-lamps.local'>http://lizard-lamps.local</a> on your home Wi-Fi. This setup hotspot closes after two minutes.</div>";
    else h += "<div class='notice'>Setup hotspot is active. Waiting to connect to <strong>" + escape(c.ssid) + "</strong>. Check your Wi-Fi details on the <a href='/?view=device'>Device page</a> if connection fails.</div>";
  }
  if (!c.targetsSet || !c.scheduleSet) h += "<div id='setup-notice' class='notice'>Finish setup: save your <a href='/?view=schedule'>day and night schedule with temperature targets</a>. Heating stays off until the schedule is saved and the clock is ready.</div>";
  const char* title = view == "monitor" ? "Your habitat, at a glance." : view == "schedule" ? "A rhythm for every day." : view == "settings" ? "Fine-tune your heating." : "Make it yours.";
  h += "<header class='head'><div><span class='eyebrow'>" + String(view == "monitor" ? "Live conditions" : "Controller settings") + "</span><h1>" + title + "</h1><span class='muted' id='controller-clock'>" + clockText() + "</span></div><span class='pill' id='connectivity'>" + String(WiFi.status() == WL_CONNECTED ? "● Wi-Fi connected" : "○ Wi-Fi offline") + "</span></header>";
  if (view == "monitor") {
    String fault = !s.sensorsHealthy ? "check both temperature probes and temperature limits." : !s.dimmerOnline ? "zero-cross detection is unavailable." : !s.clockValid ? "waiting for a valid clock." : "";
    h += "<div id='heating-fault' class='notice error' role='alert'" + String(fault.isEmpty() ? " hidden" : "") + ">" + (fault.isEmpty() ? String("") : String("Heating paused — ") + fault) + "</div><div id='clock-action'" + String(s.clockValid ? " hidden" : "") + ">" + formStart("/clock") + "<button>Use browser time</button></form><p class='muted small'>Check your phone or computer clock first.</p></div>";
    h += "<div class='grid'>";
    const char* names[] = {"Basking spot", "Ambient"};
    for (int i = 0; i < 2; ++i) {
      h += "<section class='card'><div class='row'><h3>" + String(names[i]) + "</h3><span class='pill'>Probe " + String(i + 1) + "</span></div><div class='measure'><span data-temperature='" + String(i) + "'>" + String(isfinite(s.temperatures[i]) && s.temperatures[i] != -127.0f ? String(s.temperatures[i], 1) : "—") + "</span><span class='unit'> °C</span></div><div class='row divider'><span class='muted'>Target temperature</span><strong data-target='" + String(i) + "'>" + String(isfinite(s.targets[i]) ? String(s.targets[i], 1) + "°C" : "—") + "</strong></div></section>";
    }
    h += "<section class='card accent full'><div class='row'><div><span class='eyebrow' style='color:#c6dac9'>Heating output</span><h2 id='active-period'>" + String(s.activeLamp >= 0 ? (s.activeLamp == 0 ? String("Day · Lamp 1") : String("Night · Lamp 2")) : "Waiting for schedule") + "</h2></div><span class='pill' id='power-state'>" + String(s.power > 0 ? "Heating" : "Output off") + "</span></div><div class='row'><span class='muted'>Requested power</span><span class='stat' id='power-percent'>" + String(s.power) + "%</span></div><div class='power'><span id='power-bar' style='width:" + String(s.power) + "%'></span></div><p class='muted small'>Both probes influence the selected lamp. The other lamp stays off.</p></section></div><section class='card gap'><div class='row'><h2>Today's rhythm</h2><a href='/?view=schedule'>Edit schedule →</a></div>" + timeline(c) + "<div class='row divider'><span data-start='0'>Day · Lamp 1 · " + timeText(c.starts[0]) + "</span><span data-start='1'>Night · Lamp 2 · " + timeText(c.starts[1]) + "</span></div></section><p class='footer'>Requested power is not a measured wattage.</p>";
  } else if (view == "schedule") {
    h += "<section class='card'><h2>Day & night schedule</h2><p class='muted'>Set when each period begins and the temperatures to maintain. The schedule repeats daily in Europe/London time.</p>" + timeline(c) + "</section>" + formStart("/schedule");
    const char* periods[] = {"Day · Lamp 1", "Night · Lamp 2"};
    const char* timeFields[] = {"lamp1", "lamp2"};
    const char* baskingFields[] = {"dayBasking", "nightBasking"};
    const char* ambientFields[] = {"dayAmbient", "nightAmbient"};
    for (int period = 0; period < 2; ++period) {
      h += "<section class='card'><h2>" + String(periods[period]) + "</h2><label for='" + timeFields[period] + "'>Starts at</label><input type='time' id='" + timeFields[period] + "' name='" + timeFields[period] + "' required value='" + timeText(c.starts[period]) + "'><div class='fields gap'>";
      h += "<div><label for='" + String(baskingFields[period]) + "'>Basking spot · °C</label><input type='number' id='" + baskingFields[period] + "' name='" + baskingFields[period] + "' step='0.1' min='5' max='40' required value='" + String(c.targets[period][0], 1) + "'></div>";
      h += "<div><label for='" + String(ambientFields[period]) + "'>Ambient · °C</label><input type='number' id='" + ambientFields[period] + "' name='" + ambientFields[period] + "' step='0.1' min='5' max='40' required value='" + String(c.targets[period][1], 1) + "'></div></div></section>";
    }
    h += "<p class='muted small'>Start times must differ. Day/lamp 1 runs until night/lamp 2 begins, then night runs until day begins again. Each period uses its own targets. Allowed targets: 5–40°C; heating pauses if either probe reaches 45°C.</p><button>Save schedule & temperatures</button></form>";
  } else if (view == "settings") {
    h += "<section class='card'><h2>Heating control</h2><p class='muted'>Choose gradual dimming or full-on/full-off heating. These settings apply to both lamps. Power changes fade over 10 seconds.</p>" + formStart("/control");
    h += "<label for='dimmingEnabled'>Dimming</label><select id='dimmingEnabled' name='dimmingEnabled' data-dimming-toggle><option value='1'" + String(c.dimmingEnabled ? " selected" : "") + ">On</option><option value='0'" + String(!c.dimmingEnabled ? " selected" : "") + ">Off</option></select>";
    h += "<div data-dimming-range><label for='dimmingRange'>Dimming range · °C</label><input type='number' id='dimmingRange' name='dimmingRange' step='0.1' min='0.1' max='10' required value='" + String(c.dimmingRange, 1) + "'><p class='muted small'>With dimming on, power falls from 100% to off over this many degrees below the targets.</p></div>";
    h += "<div data-restart-drop><label for='restartDrop'>Restart temperature drop · °C</label><input type='number' id='restartDrop' name='restartDrop' step='0.1' min='0.1' max='10' required value='" + String(c.restartDrop, 1) + "'><p class='muted small'>With dimming off, the lamp runs at 100% until both probes reach their targets. It stays off until either probe drops this far below its target. For example, a 30°C target and a 2°C drop restart heating at 28°C.</p></div><button>Save heating control</button></form></section>";
    h += "<section class='card'><h2>Know your probes</h2><p class='muted'>Compare readings to identify the physical probes. Addresses stay fixed across restarts.</p><div class='list'><div class='row'><span>Basking spot</span><code data-probe='0'>" + String(c.probesSet ? address(c.probes[0]) : "Not detected") + "</code></div><div class='row'><span>Ambient</span><code data-probe='1'>" + String(c.probesSet ? address(c.probes[1]) : "Not detected") + "</code></div></div>" + formStart("/probes") + "<button>Swap probe labels</button></form><p class='muted small gap'>Swap which physical probe is basking versus ambient. Day and night targets retain their labels. If replacing a probe, factory reset and set up again.</p></section>";
  } else {
    h += "<section class='card'><h2>Connection</h2><div class='list'><div class='row'><span>Dashboard address</span><a href='http://lizard-lamps.local'>lizard-lamps.local</a></div><div class='row'><span>Device IP (fallback)</span><strong>" + String(WiFi.status() == WL_CONNECTED ? WiFi.localIP().toString() : "Not connected") + "</strong></div><div class='row'><span>Network</span><strong>" + escape(c.ssid) + "</strong></div></div><div class='divider'>" + wifiForm(false, c) + "</div></section><section class='card'><h2>Dashboard access</h2>" + formStart("/admin") + "<input type='hidden' data-admin-current value='" + String(c.adminPassword[0] ? "1" : "0") + "'>" + adminFields(false, c) + "<button>Save password protection</button></form></section><section class='card'><h2>Factory reset</h2><p class='muted'>Stop heating and delete all saved Wi-Fi credentials, dashboard password, targets, schedule and probe mappings. The device will restart with an open setup hotspot. Join LizardLamps-XXXXXX and open http://192.168.4.1.</p>" + formStart("/reset") + "<label for='confirmation'>Type RESET to erase this device</label><input id='confirmation' name='confirmation' required pattern='RESET' autocomplete='off'><button class='danger'>Erase data & restart</button></form></section>";
  }
  h += "<p class='footer'><span id='live-update' role='status'>Connecting to live updates…</span><br>Independent hardware over-temperature protection is required.</p></main></div>" + ending(); sendPage(h, code);
}
void startHotspot() {
  WiFi.mode(WIFI_AP_STA);
  if (!WiFi.softAP(hotspotName.c_str())) { Serial.println("Hotspot failed"); return; }
  dns.start(53, "*", WiFi.softAPIP()); hotspot = true;
  Serial.printf("Open setup hotspot: %s | No password | Open http://192.168.4.1\n", hotspotName.c_str());
}
void connectNetwork() {
  DeviceConfig c = configSnapshot();
  if (mdnsStarted) { MDNS.end(); mdnsStarted = false; }
  WiFi.disconnect(false, false);
  if (c.ssid[0]) WiFi.begin(c.ssid, c.wifiPassword);
  disconnectedSince = millis(); connectedSince = 0; wasConnected = false;
}
void networkTick() {
  if (reconnectRequested) { reconnectRequested = false; connectNetwork(); }
  bool connected = WiFi.status() == WL_CONNECTED;
  if (connected && !wasConnected) {
    connectedSince = millis();
    configTzTime(Zone, "pool.ntp.org", "time.nist.gov");
    mdnsStarted = MDNS.begin("lizard-lamps");
    if (mdnsStarted) MDNS.addService("http", "tcp", 80);
    Serial.printf("Dashboard: http://lizard-lamps.local or http://%s (on your home Wi-Fi; username: admin)\n", WiFi.localIP().toString().c_str());
  }
  if (!connected && wasConnected) {
    disconnectedSince = millis(); connectedSince = 0;
    if (mdnsStarted) { MDNS.end(); mdnsStarted = false; }
  }
  if (!connected && !hotspot && millis() - disconnectedSince > 30000) startHotspot();
  if (connected && hotspot && millis() - connectedSince > 120000) {
    dns.stop(); WiFi.softAPdisconnect(true); WiFi.mode(WIFI_STA); hotspot = false;
  }
  wasConnected = connected;
}
void apiConnection() {
  if (!authorized()) return;
  DeviceConfig config = configSnapshot();
  bool connected = WiFi.status() == WL_CONNECTED;
  sendJson("{\"connected\":" + String(jsonBool(connected)) + ",\"ssid\":" + jsonString(config.ssid) +
           ",\"ip\":" + (connected ? jsonString(WiFi.localIP().toString()) : String("null")) +
           ",\"hotspot\":" + jsonBool(hotspot) + ",\"hostname\":\"lizard-lamps.local\"}");
}
void apiStatus() {
  if (!authorized()) return;
  DeviceStatus status = statusSnapshot();
  DeviceConfig config = configSnapshot();
  sendJson("{\"temperatures\":[" + jsonNumber(status.temperatures[0]) + "," + jsonNumber(status.temperatures[1]) +
           "],\"targets\":[" + jsonNumber(status.targets[0]) + "," + jsonNumber(status.targets[1]) +
           "],\"activeLamp\":" + String(status.activeLamp) + ",\"power\":" + String(status.power) +
           ",\"sensorsHealthy\":" + jsonBool(status.sensorsHealthy) + ",\"dimmerOnline\":" + jsonBool(status.dimmerOnline) +
           ",\"clockValid\":" + jsonBool(status.clockValid) + ",\"clock\":" + jsonString(clockText()) +
           ",\"configured\":" + jsonBool(config.targetsSet && config.scheduleSet) +
           ",\"wifiConnected\":" + jsonBool(WiFi.status() == WL_CONNECTED) + "}");
}
void apiConfig() {
  if (!authorized()) return;
  DeviceConfig config = configSnapshot();
  sendJson("{\"starts\":[" + jsonString(timeText(config.starts[0])) + "," + jsonString(timeText(config.starts[1])) +
           "],\"targets\":[[" + jsonNumber(config.targets[0][0]) + "," + jsonNumber(config.targets[0][1]) +
           "],[" + jsonNumber(config.targets[1][0]) + "," + jsonNumber(config.targets[1][1]) +
           "]],\"dimmingEnabled\":" + jsonBool(config.dimmingEnabled) + ",\"dimmingRange\":" + jsonNumber(config.dimmingRange) +
           ",\"restartDrop\":" + jsonNumber(config.restartDrop) + ",\"configured\":" + jsonBool(config.targetsSet && config.scheduleSet) + ",\"adminProtected\":" + jsonBool(config.adminPassword[0]) + ",\"probes\":[" +
           (config.probesSet ? jsonString(address(config.probes[0])) : String("null")) + "," +
           (config.probesSet ? jsonString(address(config.probes[1])) : String("null")) + "]}");
}
void webTask(void*) {
  WiFi.persistent(false);
  WiFi.mode(WIFI_STA);
  WiFi.setAutoReconnect(true);
  WiFi.setHostname("lizard-lamps");
  setenv("TZ", Zone, 1); tzset();
  DeviceConfig c = configSnapshot();
  if (!c.ssid[0]) startHotspot();
  connectNetwork();
  server.on("/favicon.svg", HTTP_GET, [] {
    server.sendHeader("Cache-Control", "public, max-age=86400");
    server.sendHeader("X-Content-Type-Options", "nosniff");
    server.send(200, "image/svg+xml", FPSTR(WebIcon));
  });
  server.on("/api/status", HTTP_GET, apiStatus);
  server.on("/api/connection", HTTP_GET, apiConnection);
  server.on("/api/config", HTTP_GET, apiConfig);
  server.on("/", HTTP_GET, [] { if (authorized()) render(); });
  server.on("/connecting", HTTP_GET, [] {
    if (!authorized()) return;
    DeviceConfig config = configSnapshot();
    bool connected = WiFi.status() == WL_CONNECTED;
    String html = beginning() + "<div class='setup-shell'><div class='welcome'>" + brand() + "<section class='card'><span class='eyebrow'>Wi-Fi setup</span><h1 id='connection-title'>" + String(connected ? "You're connected." : "Finding your network…") + "</h1><div id='live-message' class='notice' role='status' hidden></div><p class='muted'>Network: <strong id='connection-network'>" + escape(config.ssid) + "</strong></p>";
    html += "<div id='connection-success'" + String(connected ? "" : " hidden") + "><p class='muted'>Rejoin your home Wi-Fi and open <a href='http://lizard-lamps.local'>http://lizard-lamps.local</a>. If that name does not work, use:</p><p class='stat'><a data-device-link='address' href='http://" + WiFi.localIP().toString() + "'>" + WiFi.localIP().toString() + "</a></p><p class='muted'>Sign in with <strong>admin</strong> if password protection is enabled. Save your day and night schedule and temperatures to finish setting up.</p><a class='button' data-device-link='button' href='http://" + WiFi.localIP().toString() + "/'>Open dashboard on home Wi-Fi →</a><p class='muted small gap'>Still on the setup hotspot? <a href='/?view=schedule'>Continue setup here</a>. On the hotspot the dashboard is at http://192.168.4.1.</p></div>";
    html += "<div id='connection-waiting'" + String(connected ? " hidden" : "") + "><p class='muted'>Connecting… This page updates automatically.</p><p class='small muted'>If this takes longer than 30 seconds, check the network name, password and 2.4GHz support.</p><a class='button' href='/?view=device'>Check Wi-Fi details</a></div><p id='connection-error' role='status' class='small muted'></p>";
    html += "</section></div></div>" + ending(); sendPage(html);
  });
  registerForm("/wifi", [] {
    if (!validPost()) return;
    DeviceConfig candidate = configSnapshot();
    String ssid = server.arg("ssid"), password = server.arg("wifiPassword"), admin = server.arg("adminPassword");
    bool initial = !candidate.ssid[0];
    bool protect = server.arg("protectAdmin") == "1";
    if (ssid.isEmpty() || ssid.length() > 32 || password.length() > 63 || (password.length() && password.length() < 8) ||
        (initial && protect && (admin.length() < 8 || admin.length() > 63))) {
      render("Enter a network name (1–32 bytes), a Wi-Fi password of 8–63 characters (or empty for an open network), and a dashboard password of 8–63 characters if protection is enabled.", 422, "device"); return;
    }
    ssid.toCharArray(candidate.ssid, sizeof(candidate.ssid)); password.toCharArray(candidate.wifiPassword, sizeof(candidate.wifiPassword));
    if (initial) {
      memset(candidate.adminPassword, 0, sizeof(candidate.adminPassword));
      if (protect) admin.toCharArray(candidate.adminPassword, sizeof(candidate.adminPassword));
    }
    if (!saveConfig(candidate)) { render("Could not save settings. Try again.", 500, "device"); return; }
    bootstrapClock(); reconnectRequested = true; redirect("/connecting");
  });
  registerForm("/admin", [] {
    if (!validPost()) return;
    DeviceConfig candidate = configSnapshot();
    if (!candidate.ssid[0]) { render("Configure Wi-Fi first.", 422, "device"); return; }
    bool enabled = server.arg("protectAdmin") == "1";
    String password = server.arg("adminPassword");
    bool reauthenticate = enabled && (!candidate.adminPassword[0] || !password.isEmpty());
    if (enabled && ((!password.isEmpty() && (password.length() < 8 || password.length() > 63)) ||
                    (password.isEmpty() && !candidate.adminPassword[0]))) {
      render("Set a password of 8–63 characters to enable protection.", 422, "device"); return;
    }
    if (!enabled) memset(candidate.adminPassword, 0, sizeof(candidate.adminPassword));
    else if (!password.isEmpty()) password.toCharArray(candidate.adminPassword, sizeof(candidate.adminPassword));
    if (!saveConfig(candidate)) { render("Could not save password protection.", 500, "device"); return; }
    if (apiRequest) { sendJson("{\"ok\":true,\"adminProtected\":" + String(jsonBool(enabled)) + ",\"reauthenticate\":" + jsonBool(reauthenticate) + "}"); return; }
    redirect("/?view=device&saved=1");
  });
  registerForm("/schedule", [] {
    if (!validPost()) return;
    DeviceConfig candidate = configSnapshot();
    if (!candidate.ssid[0]) { render("Connect to Wi-Fi first.", 422, "schedule"); return; }
    if (!Control::parseTime(server.arg("lamp1").c_str(), candidate.starts[0]) ||
        !Control::parseTime(server.arg("lamp2").c_str(), candidate.starts[1]) || candidate.starts[0] == candidate.starts[1]) { render("Choose two different valid start times.", 422, "schedule"); return; }
    const char* fields[2][2] = {{"dayBasking", "dayAmbient"}, {"nightBasking", "nightAmbient"}};
    for (int period = 0; period < 2; ++period)
      for (int probe = 0; probe < 2; ++probe)
        if (!Control::parseTarget(server.arg(fields[period][probe]).c_str(), candidate.targets[period][probe])) {
          render("All four day/night targets must be finite numbers between 5°C and 40°C.", 422, "schedule"); return;
        }
    candidate.scheduleSet = candidate.targetsSet = true;
    if (!saveConfig(candidate)) { render("Could not save schedule and temperatures.", 500, "schedule"); return; }
    bootstrapClock(); redirect("/?view=monitor&saved=1");
  });
  registerForm("/control", [] {
    if (!validPost()) return;
    DeviceConfig candidate = configSnapshot();
    if (!candidate.ssid[0]) { render("Connect to Wi-Fi first.", 422, "settings"); return; }
    String mode = server.arg("dimmingEnabled");
    if ((mode != "0" && mode != "1") ||
        (mode == "1" && !server.hasArg("dimmingRange")) ||
        (mode == "0" && !server.hasArg("restartDrop")) ||
        (server.hasArg("dimmingRange") && !Control::parseControlRange(server.arg("dimmingRange").c_str(), candidate.dimmingRange)) ||
        (server.hasArg("restartDrop") && !Control::parseControlRange(server.arg("restartDrop").c_str(), candidate.restartDrop))) {
      render("Choose dimming on or off and temperature ranges between 0.1°C and 10°C.", 422, "settings"); return;
    }
    candidate.dimmingEnabled = mode == "1";
    if (!saveConfig(candidate)) { render("Could not save heating control.", 500, "settings"); return; }
    bootstrapClock(); redirect("/?view=settings&saved=1");
  });
  registerForm("/clock", [] {
    if (!validPost()) return;
    bootstrapClock();
    if (time(nullptr) < 1704067200) { render("Could not set the clock. Enable JavaScript and check your browser device time.", 422, "monitor"); return; }
    redirect("/?view=monitor&saved=1");
  });
  registerForm("/probes", [] {
    if (!validPost()) return;
    DeviceConfig candidate = configSnapshot();
    if (!candidate.probesSet) { render("Two probes must be detected at initial setup. Connect them and restart.", 422, "settings"); return; }
    for (int i = 0; i < 8; ++i) { uint8_t old = candidate.probes[0][i]; candidate.probes[0][i] = candidate.probes[1][i]; candidate.probes[1][i] = old; }
    if (!saveConfig(candidate)) { render("Could not save probe labels.", 500, "settings"); return; }
    redirect("/?view=settings&saved=1");
  });
  registerForm("/reset", [] {
    if (!validPost()) return;
    if (server.arg("confirmation") != "RESET") { render("Type RESET to confirm deletion.", 422, "device"); return; }
    xSemaphoreTake(stateMutex, portMAX_DELAY); resetRequested = true; maintenanceAcknowledged = false; xSemaphoreGive(stateMutex);
    rebootAt = millis() + 1000;
    if (apiRequest) { sendJson("{\"ok\":true,\"resetting\":true}"); return; }
    sendPage(beginning() + "<div class='setup-shell'><div class='card'><h1>Starting fresh.</h1><p>Heating is stopping. All saved data will be erased and the device will restart. Join the open LizardLamps-XXXXXX hotspot (no password), then open http://192.168.4.1 in your browser.</p></div></div>" + ending());
  });
  server.onNotFound([] { server.sendHeader("Location", "http://" + String(hotspot ? WiFi.softAPIP().toString() : WiFi.localIP().toString()) + "/"); server.send(302, "text/plain", "Open dashboard"); });
  server.begin();
  for (;;) {
    if (rebootAt && static_cast<int32_t>(millis() - rebootAt) >= 0) {
      xSemaphoreTake(stateMutex, portMAX_DELAY); bool stopped = maintenanceAcknowledged; xSemaphoreGive(stateMutex);
      if (stopped) {
        Preferences prefs;
        bool erased = prefs.begin("lizard", false);
        if (erased) { erased = prefs.clear(); prefs.end(); }
        if (erased) { WiFi.disconnect(true, true); ESP.restart(); }
        Serial.println("Factory reset could not erase data; heating remains off. Retry reset."); rebootAt = 0;
      }
    }
    server.handleClient();
    if (hotspot) dns.processNextRequest();
    networkTick(); delay(5);
  }
}
} // namespace
void startDeviceWeb() {
  WiFi.mode(WIFI_STA); // Enable radio entropy before generating the form token.
  uint64_t mac = ESP.getEfuseMac(); char suffix[7]; snprintf(suffix, sizeof(suffix), "%06X", static_cast<unsigned int>(mac & 0xFFFFFF));
  hotspotName = String("LizardLamps-") + suffix;
  Preferences prefs;
  if (!prefs.begin("lizard", false)) { Serial.println("Storage unavailable; web setup not started"); return; }
  prefs.remove("apPassword"); // Remove the obsolete protected-hotspot setting on upgrade.
  prefs.end();
  char token[33]; snprintf(token, sizeof(token), "%08lx%08lx%08lx%08lx", (unsigned long)esp_random(), (unsigned long)esp_random(), (unsigned long)esp_random(), (unsigned long)esp_random()); csrf = token;
  Serial.printf("Setup/recovery: join %s (no password), open http://192.168.4.1\n", hotspotName.c_str());
  // HTTP/network waits run on core 0; loop() and dimmer ownership remain on core 1.
  if (xTaskCreatePinnedToCore(webTask, "device-web", 12288, nullptr, 1, nullptr, 0) != pdPASS)
    Serial.println("Web task could not start");
}
