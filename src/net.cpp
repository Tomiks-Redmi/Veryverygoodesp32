#include "net.h"
#include "config.h"
#include "settings.h"
#include "sensors.h"
#include "logic.h"
#include "sound.h"
#include "input.h"
#include "display.h"
#include "web_page.h"
#include "clock.h"
#include "tg.h"
#include "leds.h"
#include <WiFi.h>
#include <WebServer.h>
#include <ESPmDNS.h>
#include <Update.h>

static WebServer server(80);
static bool active = false;
static bool serverStarted = false;
static uint32_t staRetryAt = 0;

bool   netWifiActive()   { return active; }
bool   netStaConnected() { return active && WiFi.status() == WL_CONNECTED; }
String netApIp()         { return active ? WiFi.softAPIP().toString() : String("-"); }
String netStaIp()        { return netStaConnected() ? WiFi.localIP().toString() : String("-"); }
int    netClients()      { return active ? WiFi.softAPgetStationNum() : 0; }
int    netRssi()         { return netStaConnected() ? WiFi.RSSI() : 0; }

// ---------------- JSON ----------------
static void jNum(String &s, const char *key, float v, uint8_t dec = 1) {
  s += '"'; s += key; s += "\":";
  if (isnan(v)) s += "null";
  else s += String(v, (unsigned int)dec);
  s += ',';
}
static void jInt(String &s, const char *key, long v) { s += '"'; s += key; s += "\":"; s += v; s += ','; }
static void jBool(String &s, const char *key, bool v) { s += '"'; s += key; s += "\":"; s += v ? "true" : "false"; s += ','; }
static void jStr(String &s, const char *key, const char *v) {
  s += '"'; s += key; s += "\":\"";
  for (const char *p = v; *p; p++) {
    if (*p == '"' || *p == '\\') s += '\\';
    if ((uint8_t)*p >= 0x20) s += *p;
  }
  s += "\",";
}
static void jEnd(String &s) {
  if (s.endsWith(",")) s.remove(s.length() - 1);
}

static void handleData() {
  String s;
  s.reserve(900);
  s = "{";
  jNum(s, "dsT", S.dsOk ? S.dsT[0] : NAN);
  jInt(s, "dsCount", S.dsCount);
  s += "\"dsAll\":[";
  for (uint8_t i = 0; i < S.dsCount; i++) {
    if (i) s += ',';
    s += isnan(S.dsT[i]) ? String("null") : String(S.dsT[i], 2);
  }
  s += "],";
  jNum(s, "dhtT", S.dhtT);
  jNum(s, "dhtH", S.dhtH, 0);
  jNum(s, "dew", S.dewPoint);
  jNum(s, "dist", S.distCm);
  jBool(s, "motion", S.motion);
  jInt(s, "motionCount", S.motionCount);
  jInt(s, "motionAgo", S.lastMotionMs ? (long)((millis() - S.lastMotionMs) / 1000) : -1);
  jInt(s, "analog", cfg.analogEnabled ? S.analogPct : -1);
  jInt(s, "joyX", joy.x);
  jInt(s, "joyY", joy.y);
  jBool(s, "joy", joy.present && cfg.joyEnabled);
  jInt(s, "sec", secState());
  jStr(s, "secName", secStateName());
  jInt(s, "armLeft", secArmingLeftSec());
  jInt(s, "alarms", secAlarmCount());
  jStr(s, "trigger", secLastTrigger());
  jBool(s, "relay", relayIsOn());
  jInt(s, "relayMode", cfg.relayMode);
  jStr(s, "relayModeName", relayModeName(cfg.relayMode));
  jNum(s, "target", cfg.thermoTarget);
  jInt(s, "uptime", millis() / 1000);
  jInt(s, "heap", ESP.getFreeHeap());
  jInt(s, "rssi", netRssi());
  jBool(s, "fahrenheit", cfg.fahrenheit);
  char hm[8];
  clockFmtHM(hm, sizeof(hm));
  jStr(s, "time", hm);
  jStr(s, "tg", tgStatus());
  jStr(s, "leds", ledsModeName(cfg.ledsMode));
  jEnd(s);
  s += '}';
  server.send(200, "application/json", s);
}

static void handleHistory() {
  int id = server.hasArg("id") ? server.arg("id").toInt() : 0;
  if (id < 0 || id >= H_COUNT) id = 0;
  const History &h = hist[id];
  String s;
  s.reserve(h.count * 7 + 40);
  s = "{\"step\":";
  s += GRAPH_STEPS_SEC[min<uint8_t>(cfg.graphStepIdx, GRAPH_STEPS_COUNT - 1)];
  s += ",\"v\":[";
  for (int i = 0; i < h.count; i++) {
    if (i) s += ',';
    float v = h.at(i);
    s += isnan(v) ? String("null") : String(v, 1);
  }
  s += "]}";
  server.send(200, "application/json", s);
}

static void handleLog() {
  String s = "[";
  uint32_t now = millis();
  for (uint8_t i = 0; i < logCount(); i++) {
    const LogEntry &e = logGet(i);
    if (i) s += ',';
    s += "{";
    jInt(s, "ago", (now - e.ms) / 1000);
    jStr(s, "t", e.text);
    jEnd(s);
    s += "}";
  }
  s += "]";
  server.send(200, "application/json", s);
}

static void handleCmd() {
  String c = server.arg("c");
  if (c == "arm") secArm();
  else if (c == "disarm") secDisarm();
  else if (c == "relay_on") relaySetManual(true);
  else if (c == "relay_off") relaySetManual(false);
  else if (c == "beep") beep(2000, 150, true);
  else if (c == "ev") inputPush((Ev)constrain(server.arg("e").toInt(), 1, 6));
  else if (c == "toast") toast(server.arg("t").c_str(), 3000);
  else if (c == "reboot") { server.send(200, "text/plain", "ok"); delay(300); ESP.restart(); }
  else { server.send(400, "text/plain", "unknown"); return; }
  if (c == "relay_on" || c == "relay_off") settingsSave();
  server.send(200, "text/plain", "ok");
}

static void handleGetSettings() {
  String s = "{";
  jInt(s, "contrast", cfg.contrast);
  jBool(s, "sound", cfg.sound);
  jBool(s, "fahrenheit", cfg.fahrenheit);
  jNum(s, "tempHigh", cfg.tempHigh);
  jNum(s, "tempLow", cfg.tempLow);
  jBool(s, "tempAlerts", cfg.tempAlerts);
  jInt(s, "alarmDistCm", cfg.alarmDistCm);
  jInt(s, "armDelaySec", cfg.armDelaySec);
  jBool(s, "alarmUsePir", cfg.alarmUsePir);
  jBool(s, "alarmUseSonar", cfg.alarmUseSonar);
  jInt(s, "relayMode", cfg.relayMode);
  jNum(s, "thermoTarget", cfg.thermoTarget);
  jNum(s, "thermoHyst", cfg.thermoHyst);
  jInt(s, "motionLightSec", cfg.motionLightSec);
  jInt(s, "graphStepIdx", cfg.graphStepIdx);
  jStr(s, "staSsid", cfg.staSsid);
  jInt(s, "tzOffset", cfg.tzOffset);
  jInt(s, "alarmClockOn", cfg.alarmClockOn);
  char al[8];
  snprintf(al, sizeof(al), "%02d:%02d", cfg.alarmHour, cfg.alarmMin);
  jStr(s, "alarmTime", al);
  jInt(s, "tgOn", cfg.tgOn);
  jInt(s, "tgMotion", cfg.tgMotion);
  jStr(s, "tgChat", cfg.tgChat);
  jBool(s, "tgHasToken", cfg.tgToken[0] != 0);
  jInt(s, "ledsMode", cfg.ledsMode);
  jInt(s, "ledsBright", cfg.ledsBright);
  jInt(s, "ledsCount", cfg.ledsCount);
  jInt(s, "servoRadar", cfg.servoRadar);
  jEnd(s);
  s += "}";
  server.send(200, "application/json", s);
}

static void handlePostSettings() {
  bool wifiChanged = false;
  auto f = [](const char *k, float &v, float lo, float hi) {
    if (server.hasArg(k)) v = constrain(server.arg(k).toFloat(), lo, hi);
  };
  auto b = [](const char *k, bool &v) {
    if (server.hasArg(k)) v = server.arg(k) == "1" || server.arg(k) == "true";
  };
  if (server.hasArg("contrast"))    cfg.contrast = constrain(server.arg("contrast").toInt(), 0, 255);
  b("sound", cfg.sound);
  b("fahrenheit", cfg.fahrenheit);
  f("tempHigh", cfg.tempHigh, -40, 125);
  f("tempLow", cfg.tempLow, -40, 125);
  b("tempAlerts", cfg.tempAlerts);
  if (server.hasArg("alarmDistCm")) cfg.alarmDistCm = constrain(server.arg("alarmDistCm").toInt(), 0, 400);
  if (server.hasArg("armDelaySec")) cfg.armDelaySec = constrain(server.arg("armDelaySec").toInt(), 0, 120);
  b("alarmUsePir", cfg.alarmUsePir);
  b("alarmUseSonar", cfg.alarmUseSonar);
  if (server.hasArg("relayMode"))   cfg.relayMode = constrain(server.arg("relayMode").toInt(), 0, RELAY_MODE_COUNT - 1);
  f("thermoTarget", cfg.thermoTarget, -20, 100);
  f("thermoHyst", cfg.thermoHyst, 0.1, 10);
  if (server.hasArg("motionLightSec")) cfg.motionLightSec = constrain(server.arg("motionLightSec").toInt(), 1, 3600);
  if (server.hasArg("graphStepIdx")) cfg.graphStepIdx = constrain(server.arg("graphStepIdx").toInt(), 0, GRAPH_STEPS_COUNT - 1);
  if (server.hasArg("tzOffset")) { cfg.tzOffset = constrain(server.arg("tzOffset").toInt(), -12, 14); clockApplyTz(); }
  b("alarmClockOn", cfg.alarmClockOn);
  if (server.hasArg("alarmTime")) {
    String t = server.arg("alarmTime");  // "ЧЧ:ММ"
    int c = t.indexOf(':');
    if (c > 0) {
      cfg.alarmHour = constrain(t.substring(0, c).toInt(), 0, 23);
      cfg.alarmMin = constrain(t.substring(c + 1).toInt(), 0, 59);
    }
  }
  b("tgOn", cfg.tgOn);
  b("tgMotion", cfg.tgMotion);
  if (server.hasArg("tgChat")) strlcpy(cfg.tgChat, server.arg("tgChat").c_str(), sizeof(cfg.tgChat));
  if (server.hasArg("tgToken") && server.arg("tgToken").length()) {
    strlcpy(cfg.tgToken, server.arg("tgToken").c_str(), sizeof(cfg.tgToken));
    cfg.tgChat[0] = 0;  // новый бот — привязать заново
  }
  if (server.hasArg("ledsMode"))   cfg.ledsMode = constrain(server.arg("ledsMode").toInt(), 0, LEDS_MODE_COUNT - 1);
  if (server.hasArg("ledsBright")) cfg.ledsBright = constrain(server.arg("ledsBright").toInt(), 5, 255);
  if (server.hasArg("ledsCount"))  cfg.ledsCount = constrain(server.arg("ledsCount").toInt(), 0, 64);
  b("servoRadar", cfg.servoRadar);
  ledsApply();
  if (server.hasArg("staSsid")) {
    strlcpy(cfg.staSsid, server.arg("staSsid").c_str(), sizeof(cfg.staSsid));
    if (server.hasArg("staPass") && server.arg("staPass").length())
      strlcpy(cfg.staPass, server.arg("staPass").c_str(), sizeof(cfg.staPass));
    if (!cfg.staSsid[0]) cfg.staPass[0] = 0;
    wifiChanged = true;
  }
  settingsSave();
  settingsApply();
  server.send(200, "text/plain", "ok");
  toast("Настройки из веб", 1500);
  if (wifiChanged) { staRetryAt = millis() + 500; }
}

static void handleUpdateDone() {
  bool ok = !Update.hasError();
  server.sendHeader("Connection", "close");
  server.send(200, "text/plain", ok ? "OK. Перезагрузка..." : "Ошибка обновления");
  if (ok) { delay(500); ESP.restart(); }
}

static void handleUpdateUpload() {
  HTTPUpload &up = server.upload();
  if (up.status == UPLOAD_FILE_START) {
    toast("Обновление...", 60000);
    if (!Update.begin(UPDATE_SIZE_UNKNOWN)) Update.printError(Serial);
  } else if (up.status == UPLOAD_FILE_WRITE) {
    if (Update.write(up.buf, up.currentSize) != up.currentSize) Update.printError(Serial);
  } else if (up.status == UPLOAD_FILE_END) {
    if (!Update.end(true)) Update.printError(Serial);
  }
}

// Время с телефона/компьютера (если нет интернета для NTP)
static void handleTime() {
  uint32_t t = server.arg("t").toInt();
  if (t > 1700000000UL && (!clockValid() || !netStaConnected())) {
    clockSetEpoch(t);
    logEvent("Время получено из браузера");
  }
  server.send(200, "text/plain", "ok");
}

static void startServer() {
  if (serverStarted) return;
  server.on("/", HTTP_GET, [] { server.send_P(200, "text/html; charset=utf-8", WEB_PAGE); });
  server.on("/api/data", HTTP_GET, handleData);
  server.on("/api/history", HTTP_GET, handleHistory);
  server.on("/api/log", HTTP_GET, handleLog);
  server.on("/api/cmd", HTTP_POST, handleCmd);
  server.on("/api/settings", HTTP_GET, handleGetSettings);
  server.on("/api/settings", HTTP_POST, handlePostSettings);
  server.on("/api/time", HTTP_POST, handleTime);
  server.on("/update", HTTP_POST, handleUpdateDone, handleUpdateUpload);
  server.onNotFound([] { server.send(404, "text/plain", "Not found"); });
  server.begin();
  serverStarted = true;
}

static void startSta() {
  if (cfg.staSsid[0]) {
    WiFi.begin(cfg.staSsid, cfg.staPass);
    logEvent("WiFi: подключение к %s", cfg.staSsid);
  }
}

void netApply() {
  if (cfg.wifiOn && !active) {
    WiFi.mode(WIFI_AP_STA);
    WiFi.setSleep(false);
    WiFi.softAP(WIFI_AP_SSID, WIFI_AP_PASS);
    startSta();
    startServer();
    if (MDNS.begin(MDNS_NAME)) MDNS.addService("http", "tcp", 80);
    active = true;
    logEvent("WiFi AP %s", WiFi.softAPIP().toString().c_str());
  } else if (!cfg.wifiOn && active) {
    MDNS.end();
    WiFi.softAPdisconnect(true);
    WiFi.disconnect(true);
    WiFi.mode(WIFI_OFF);
    active = false;
    logEvent("WiFi выключен");
  }
}

void netBegin() {
  WiFi.persistent(false);
  netApply();
}

void netUpdate() {
  if (!active) return;
  server.handleClient();

  static bool wasConnected = false;
  bool c = WiFi.status() == WL_CONNECTED;
  if (c != wasConnected) {
    wasConnected = c;
    if (c) {
      clockStartNtp();
      logEvent("WiFi IP %s", WiFi.localIP().toString().c_str());
      char b[40];
      snprintf(b, sizeof(b), "IP %s", WiFi.localIP().toString().c_str());
      toast(b, 3000);
    }
  }
  if (staRetryAt && (int32_t)(millis() - staRetryAt) >= 0) {
    staRetryAt = 0;
    WiFi.disconnect();
    startSta();
  }
}
