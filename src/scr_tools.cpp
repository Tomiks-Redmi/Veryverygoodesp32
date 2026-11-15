// Инструменты: I2C сканер, WiFi сканер, секундомер, фонарик, WiFi-инфо, о системе
#include "screens.h"
#include "display.h"
#include "sensors.h"
#include "settings.h"
#include "sound.h"
#include "net.h"
#include "logic.h"
#include "config.h"
#include <Wire.h>
#include <WiFi.h>

// =====================================================================
//  I2C сканер — удобно, когда модулей «куча»
// =====================================================================
static const char *i2cGuess(uint8_t a) {
  switch (a) {
    case 0x3C: case 0x3D: return "OLED SSD1306";
    case 0x27: case 0x3F: return "LCD PCF8574";
    case 0x20: case 0x21: case 0x22: return "PCF8574/MCP23017";
    case 0x23: return "BH1750/PCF8574";
    case 0x29: return "VL53L0X";
    case 0x40: return "INA219/HTU21/PCA9685";
    case 0x44: case 0x45: return "SHT3x";
    case 0x48: case 0x49: case 0x4A: case 0x4B: return "ADS1115/LM75";
    case 0x50: case 0x51: case 0x52: case 0x53: case 0x54: case 0x55: case 0x56: case 0x57: return "EEPROM 24Cxx";
    case 0x5A: return "MLX90614/MPR121";
    case 0x5C: return "BH1750/AM2320";
    case 0x60: return "Si5351/MPL3115";
    case 0x68: return "MPU6050/DS3231";
    case 0x69: return "MPU6050 (AD0)";
    case 0x76: case 0x77: return "BME280/BMP280";
    case 0x38: return "AHT10/AHT20";
    case 0x39: return "APDS9960/TSL2561";
    case 0x1E: return "HMC5883L";
    case 0x0D: return "QMC5883L";
  }
  return "?";
}

class I2CScan : public Screen {
  uint8_t found_[32];
  uint8_t n_ = 0;
  int top_ = 0;

  void scan() {
    n_ = 0;
    for (uint8_t a = 1; a < 127 && n_ < sizeof(found_); a++) {
      Wire.beginTransmission(a);
      if (Wire.endTransmission() == 0) found_[n_++] = a;
    }
    top_ = 0;
  }

 public:
  void enter() override { scan(); }
  void event(Ev e) override {
    if (e == EV_OK) { scan(); toast("Пересканировано", 800); return; }
    if (e == EV_NEXT) { top_ = min(top_ + 1, max(0, (int)n_ - 4)); return; }
    if (e == EV_PREV) { top_ = max(top_ - 1, 0); return; }
    Screen::event(e);
  }
  void draw() override {
    char b[40];
    snprintf(b, sizeof(b), "I2C: найдено %d", n_);
    drawHeader(b);
    if (!n_) { gfx.setFont(F_MAIN); drawCenteredUTF8(38, "Ничего нет"); return; }
    for (int i = 0; i < 4 && top_ + i < n_; i++) {
      uint8_t a = found_[top_ + i];
      int y = HEADER_H + 11 + i * 13;
      gfx.setFont(F_BOLD);
      snprintf(b, sizeof(b), "0x%02X", a);
      gfx.drawStr(2, y, b);
      gfx.setFont(F_SMALL);
      gfx.drawUTF8(36, y, i2cGuess(a));
    }
    drawScrollbar(top_, max(1, (int)n_ - 3));
  }
};

Screen *scrI2CScan() { static I2CScan s; return &s; }

// =====================================================================
//  WiFi сканер
// =====================================================================
class WifiScan : public Screen {
  int top_ = 0;
  bool scanning_ = false;
  int n_ = 0;
  bool startedWifi_ = false;

  void start() {
    if (!netWifiActive()) { WiFi.mode(WIFI_STA); startedWifi_ = true; }
    WiFi.scanDelete();
    WiFi.scanNetworks(true, true);
    scanning_ = true;
    n_ = 0;
    top_ = 0;
  }

 public:
  void enter() override { start(); }
  void leave() override {
    WiFi.scanDelete();
    if (startedWifi_ && !netWifiActive()) WiFi.mode(WIFI_OFF);
    startedWifi_ = false;
  }
  void event(Ev e) override {
    if (e == EV_OK && !scanning_) { start(); return; }
    if (e == EV_NEXT) { top_ = min(top_ + 1, max(0, n_ - 4)); return; }
    if (e == EV_PREV) { top_ = max(top_ - 1, 0); return; }
    Screen::event(e);
  }
  void update() override {
    if (!scanning_) return;
    int r = WiFi.scanComplete();
    if (r >= 0) { n_ = r; scanning_ = false; }
    else if (r == WIFI_SCAN_FAILED) { n_ = 0; scanning_ = false; }
  }
  void draw() override {
    char b[40];
    if (scanning_) {
      drawHeader("WiFi сканер");
      gfx.setFont(F_MAIN);
      drawCenteredUTF8(36, "Сканирую...");
      int x = (millis() / 20) % 100;
      gfx.drawBox(14 + x, 44, 10, 3);
      gfx.drawFrame(12, 42, 104, 7);
      return;
    }
    snprintf(b, sizeof(b), "WiFi: сетей %d", n_);
    drawHeader(b);
    for (int i = 0; i < 4 && top_ + i < n_; i++) {
      int k = top_ + i;
      int y = HEADER_H + 11 + i * 13;
      int rssi = WiFi.RSSI(k);
      int bars = rssi > -55 ? 4 : rssi > -67 ? 3 : rssi > -78 ? 2 : 1;
      for (int j = 0; j < 4; j++) {
        int h = 2 + j * 2;
        if (j < bars) gfx.drawBox(1 + j * 3, y - h, 2, h);
        else gfx.drawPixel(1 + j * 3, y - 1);
      }
      gfx.setFont(F_SMALL);
      String ssid = WiFi.SSID(k);
      if (!ssid.length()) ssid = "<скрытая>";
      if (ssid.length() > 15) ssid = ssid.substring(0, 15);
      gfx.drawUTF8(15, y, ssid.c_str());
      snprintf(b, sizeof(b), "%d%s", rssi, WiFi.encryptionType(k) == WIFI_AUTH_OPEN ? " o" : "");
      gfx.setFont(F_TINY);
      drawRightUTF8(SCR_W - 4, y, b);
    }
    drawScrollbar(top_, max(1, n_ - 3));
  }
};

Screen *scrWifiScan() { static WifiScan s; return &s; }

// =====================================================================
//  Секундомер с кругами
// =====================================================================
class Stopwatch : public Screen {
  bool run_ = false;
  uint32_t start_ = 0, acc_ = 0;
  uint32_t laps_[3] = {0};
  uint8_t nl_ = 0;
  uint32_t elapsed() const { return acc_ + (run_ ? millis() - start_ : 0); }

 public:
  bool keepAwake() override { return run_; }
  void event(Ev e) override {
    switch (e) {
      case EV_OK:
        if (run_) { acc_ += millis() - start_; run_ = false; }
        else { start_ = millis(); run_ = true; }
        beep(2600, 15);
        break;
      case EV_NEXT: case EV_RIGHT:  // круг
        if (run_) { memmove(laps_ + 1, laps_, sizeof(laps_) - sizeof(laps_[0])); laps_[0] = elapsed(); if (nl_ < 3) nl_++; }
        break;
      case EV_PREV:  // сброс
        if (!run_) { acc_ = 0; nl_ = 0; }
        break;
      default: Screen::event(e);
    }
  }
  void draw() override {
    drawHeader(run_ ? "Секундомер" : "Секундомер (стоп)");
    uint32_t t = elapsed();
    char b[20];
    snprintf(b, sizeof(b), "%02lu:%02lu.%01lu", (unsigned long)(t / 60000), (unsigned long)(t / 1000 % 60), (unsigned long)(t / 100 % 10));
    gfx.setFont(F_BIG);
    int w = gfx.getStrWidth(b);
    gfx.drawStr((SCR_W - w) / 2, 38, b);
    gfx.setFont(F_TINY);
    for (int i = 0; i < nl_; i++) {
      uint32_t l = laps_[i];
      snprintf(b, sizeof(b), "%d) %02lu:%02lu.%02lu", nl_ - i, (unsigned long)(l / 60000), (unsigned long)(l / 1000 % 60), (unsigned long)(l / 10 % 100));
      gfx.drawStr(i * 43, 48, b);
    }
    gfx.drawUTF8(0, 63, "OK старт/стоп  > круг  < сброс");
  }
};

Screen *scrStopwatch() { static Stopwatch s; return &s; }

// =====================================================================
//  Фонарик — весь экран белый, яркость максимальная
// =====================================================================
class Flashlight : public Screen {
 public:
  bool keepAwake() override { return true; }
  void enter() override { gfx.setContrast(255); }
  void leave() override { displayApplySettings(); }
  void draw() override { gfx.drawBox(0, 0, SCR_W, SCR_H); }
};

Screen *scrFlashlight() { static Flashlight s; return &s; }

// =====================================================================
//  WiFi: информация и включение
// =====================================================================
class WifiInfo : public Screen {
 public:
  void event(Ev e) override {
    if (e == EV_OK) {
      cfg.wifiOn = !cfg.wifiOn;
      settingsSave();
      netApply();
      toast(cfg.wifiOn ? "WiFi включён" : "WiFi выключен", 1200);
      return;
    }
    Screen::event(e);
  }
  void draw() override {
    drawHeader("WiFi");
    gfx.setFont(F_SMALL);
    char b[48];
    if (!netWifiActive()) {
      drawArt(Icons::WIFI, 16, 4, 20, 2);
      gfx.drawLine(4, 50, 36, 18);
      gfx.drawUTF8(44, 30, "WiFi выключен");
      gfx.drawUTF8(44, 42, "OK - включить");
      return;
    }
    snprintf(b, sizeof(b), "Сеть: %s", WIFI_AP_SSID);  gfx.drawUTF8(0, 19, b);
    snprintf(b, sizeof(b), "Пароль: %s", WIFI_AP_PASS); gfx.drawUTF8(0, 28, b);
    snprintf(b, sizeof(b), "http://%s", netApIp().c_str()); gfx.drawUTF8(0, 37, b);
    if (cfg.staSsid[0]) {
      if (netStaConnected()) snprintf(b, sizeof(b), "Роутер: %s %ddBm", netStaIp().c_str(), netRssi());
      else snprintf(b, sizeof(b), "Роутер: подключаюсь...");
    } else {
      snprintf(b, sizeof(b), "Роутер: не настроен");
    }
    gfx.drawUTF8(0, 46, b);
    snprintf(b, sizeof(b), "Клиентов: %d  %s.local", netClients(), MDNS_NAME);
    gfx.drawUTF8(0, 55, b);
    gfx.setFont(F_TINY);
    drawRightUTF8(SCR_W, 63, "OK - выключить");
  }
};

Screen *scrWifiInfo() { static WifiInfo s; return &s; }

// =====================================================================
//  О системе + статус модулей
// =====================================================================
class InfoScreen : public Screen {
  int page_ = 0;

 public:
  void event(Ev e) override {
    if (e == EV_NEXT || e == EV_PREV || e == EV_OK || e == EV_RIGHT) { page_ ^= 1; return; }
    Screen::event(e);
  }
  void draw() override {
    char b[40];
    if (page_ == 0) {
      drawHeader("О системе 1/2");
      gfx.setFont(F_SMALL);
      snprintf(b, sizeof(b), "%s v%s", FW_NAME, FW_VERSION); gfx.drawStr(0, 19, b);
      snprintf(b, sizeof(b), "%s rev%d %dМГц", ESP.getChipModel(), (int)ESP.getChipRevision(), (int)ESP.getCpuFreqMHz()); gfx.drawUTF8(0, 28, b);
      snprintf(b, sizeof(b), "RAM своб.: %lu КБ", (unsigned long)(ESP.getFreeHeap() / 1024)); gfx.drawUTF8(0, 37, b);
      snprintf(b, sizeof(b), "Flash: %lu МБ", (unsigned long)(ESP.getFlashChipSize() / 1048576)); gfx.drawUTF8(0, 46, b);
      char d[20];
      fmtDuration(d, sizeof(d), millis() / 1000);
      snprintf(b, sizeof(b), "Работает: %s", d); gfx.drawUTF8(0, 55, b);
      gfx.setFont(F_TINY);
      drawRightUTF8(SCR_W, 63, "> модули");
    } else {
      drawHeader("Модули 2/2");
      struct { const char *n; bool ok; } m[] = {
        {"DS18B20", S.dsOk}, {"DHT11", S.dhtOk}, {"Сонар", S.sonarOk},
        {"Джойстик", joy.present && cfg.joyEnabled}, {"PIR", cfg.pirEnabled}, {"WiFi", netWifiActive()},
      };
      gfx.setFont(F_SMALL);
      for (int i = 0; i < 6; i++) {
        int x = (i % 2) * 64, y = 21 + (i / 2) * 14;
        if (m[i].ok) gfx.drawDisc(x + 4, y - 3, 3); else gfx.drawCircle(x + 4, y - 3, 3);
        gfx.drawUTF8(x + 11, y, m[i].n);
      }
      gfx.setFont(F_TINY);
      snprintf(b, sizeof(b), "DS18B20 на шине: %d шт.", S.dsCount);
      drawCenteredUTF8(63, b);
    }
  }
};

Screen *scrInfo() { static InfoScreen s; return &s; }
