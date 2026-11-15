// =====================================================================
//  VeryGood ESP32 — многофункциональная прошивка-«комбайн»
//  OLED 0.96" + энкодер + DHT11 + DS18B20 + HC-SR04 + PIR + джойстик
//  + бузер + реле + веб-панель по WiFi
// =====================================================================
#include <Arduino.h>
#include <Wire.h>
#include <esp_system.h>
#include "config.h"
#include "settings.h"
#include "display.h"
#include "input.h"
#include "sensors.h"
#include "sound.h"
#include "logic.h"
#include "net.h"
#include "ui.h"
#include "screens.h"

// ---------------- Заставка при загрузке ----------------
static void splash(const char *status, float progress) {
  gfx.clearBuffer();
  gfx.setFont(F_TITLE);
  drawCenteredUTF8(20, "VeryGood");
  gfx.setFont(F_BOLD);
  drawCenteredUTF8(34, "ESP32");
  gfx.drawHLine(24, 38, 80);
  gfx.setFont(F_SMALL);
  drawCenteredUTF8(50, status);
  drawProgress(14, 55, 100, 7, progress);
  gfx.sendBuffer();
}

static void splashChecks() {
  struct { const char *name; bool ok; } items[] = {
    {"Энкодер", true},
    {"DS18B20", S.dsCount > 0},
    {"Джойстик", joy.present},
    {"PIR", cfg.pirEnabled},
    {"WiFi", cfg.wifiOn},
  };
  gfx.clearBuffer();
  drawHeader("Проверка модулей");
  gfx.setFont(F_SMALL);
  for (int i = 0; i < 5; i++) {
    int x = (i % 2) * 64 + 2, y = 21 + (i / 2) * 13;
    if (items[i].ok) gfx.drawDisc(x + 3, y - 3, 3); else gfx.drawCircle(x + 3, y - 3, 3);
    gfx.drawUTF8(x + 10, y, items[i].name);
  }
  gfx.setFont(F_TINY);
  drawRightUTF8(SCR_W, 63, "v" FW_VERSION);
  gfx.sendBuffer();
}

// ---------------- Консоль в Serial ----------------
static void printStatus() {
  Serial.printf("DS18B20: %s (%d шт.)\n", S.dsOk ? String(S.dsT[0], 2).c_str() : "--", S.dsCount);
  Serial.printf("DHT11:   %s C, %s %%\n", S.dhtOk ? String(S.dhtT, 1).c_str() : "--", S.dhtOk ? String(S.dhtH, 0).c_str() : "--");
  Serial.printf("Сонар:   %s см\n", S.sonarOk ? String(S.distCm, 1).c_str() : "--");
  Serial.printf("PIR:     %s, срабатываний %lu\n", S.motion ? "ДВИЖЕНИЕ" : "тихо", (unsigned long)S.motionCount);
  Serial.printf("Джойст.: %s x=%d y=%d\n", joy.present ? "есть" : "нет", joy.x, joy.y);
  Serial.printf("Охрана:  %s, тревог %lu\n", secStateName(), (unsigned long)secAlarmCount());
  Serial.printf("Реле:    %s (%s)\n", relayIsOn() ? "ВКЛ" : "ВЫКЛ", relayModeName(cfg.relayMode));
  Serial.printf("WiFi:    AP %s, STA %s\n", netApIp().c_str(), netStaIp().c_str());
  Serial.printf("RAM:     %lu байт свободно\n", (unsigned long)ESP.getFreeHeap());
}

static void serialCli() {
  static String line;
  while (Serial.available()) {
    char c = Serial.read();
    if (c == '\r') continue;
    if (c != '\n') { if (line.length() < 120) line += c; continue; }
    line.trim();
    String cmd = line, arg;
    int sp = line.indexOf(' ');
    if (sp > 0) { cmd = line.substring(0, sp); arg = line.substring(sp + 1); arg.trim(); }
    line = "";
    if (!cmd.length()) continue;

    if (cmd == "help") {
      Serial.println(F("Команды: status, arm, disarm, beep, relay on|off|auto, wifi on|off,\n"
                       "         sta <ssid> <пароль>, log, reset, reboot"));
    } else if (cmd == "status") printStatus();
    else if (cmd == "arm") secArm();
    else if (cmd == "disarm") secDisarm();
    else if (cmd == "beep") beep(2000, 200, true);
    else if (cmd == "relay") {
      if (arg == "on") relaySetManual(true);
      else if (arg == "off") relaySetManual(false);
      else if (arg == "auto") cfg.relayMode = RELAY_HEAT;
      settingsSave();
    } else if (cmd == "wifi") {
      cfg.wifiOn = arg == "on";
      settingsSave();
      netApply();
    } else if (cmd == "sta") {
      int s2 = arg.indexOf(' ');
      String ssid = s2 > 0 ? arg.substring(0, s2) : arg;
      String pass = s2 > 0 ? arg.substring(s2 + 1) : "";
      strlcpy(cfg.staSsid, ssid.c_str(), sizeof(cfg.staSsid));
      strlcpy(cfg.staPass, pass.c_str(), sizeof(cfg.staPass));
      settingsSave();
      Serial.println(F("Сохранено, перезагрузка..."));
      delay(300);
      ESP.restart();
    } else if (cmd == "log") {
      for (int i = logCount() - 1; i >= 0; i--) Serial.printf("  -%lus  %s\n", (unsigned long)((millis() - logGet(i).ms) / 1000), logGet(i).text);
    } else if (cmd == "reset") { settingsReset(); Serial.println(F("Настройки сброшены")); }
    else if (cmd == "reboot") ESP.restart();
    else Serial.println(F("Неизвестная команда, наберите help"));
  }
}

// ---------------- setup / loop ----------------
void setup() {
  Serial.begin(115200);
  delay(50);
  Serial.println();
  Serial.println(F("=== " FW_NAME " v" FW_VERSION " ==="));

  settingsLoad();
  Wire.begin(PIN_I2C_SDA, PIN_I2C_SCL);
  displayBegin();
  splash("Запуск...", 0.1f);

  soundBegin();
  splash("Энкодер и джойстик", 0.3f);
  inputBegin();

  splash("Датчики", 0.5f);
  sensorsBegin();
  logicBegin();

  splash("WiFi", 0.75f);
  netBegin();

  splash("Готово!", 1.0f);
  randomSeed(esp_random());
  sfx(SFX_START);
  delay(400);
  splashChecks();
  delay(1300);

  uiBegin(scrMainMenu());
  inputClear();
  inputTouch();
  Serial.println(F("Наберите help для списка команд"));
}

void loop() {
  inputUpdate();
  sensorsUpdate();
  logicUpdate();
  soundUpdate();
  netUpdate();
  uiLoop();
  serialCli();
  delay(1);
}
