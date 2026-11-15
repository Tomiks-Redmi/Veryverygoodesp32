// Охрана, ввод PIN, реле/термостат, журнал
#include "screens.h"
#include "display.h"
#include "sensors.h"
#include "settings.h"
#include "sound.h"
#include "logic.h"
#include "clock.h"

// =====================================================================
//  Ввод PIN-кода
// =====================================================================
class PinEntry : public Screen {
  uint8_t d_[4] = {0, 0, 0, 0};
  uint8_t pos_ = 0;
  uint8_t tries_ = 0;
  void (*onOk_)() = nullptr;
  bool setMode_ = false;  // режим задания нового PIN

 public:
  void start(void (*onOk)(), bool setMode = false) {
    onOk_ = onOk; setMode_ = setMode; memset(d_, 0, 4); pos_ = 0; tries_ = 0;
  }
  void event(Ev e) override {
    switch (e) {
      case EV_NEXT: case EV_RIGHT: d_[pos_] = (d_[pos_] + 1) % 10; break;
      case EV_PREV: case EV_LEFT:  d_[pos_] = (d_[pos_] + 9) % 10; break;
      case EV_OK:
        if (++pos_ < 4) break;
        if (setMode_) {
          cfg.pinCode = d_[0] * 1000 + d_[1] * 100 + d_[2] * 10 + d_[3];
          settingsSave();
          uiPop();
          toast("PIN сохранён", 1200);
          break;
        }
        if (d_[0] * 1000 + d_[1] * 100 + d_[2] * 10 + d_[3] == cfg.pinCode) {
          tries_ = 0;
          uiPop();
          if (onOk_) onOk_();
        } else {
          tries_++;
          sfx(SFX_ERROR);
          toast("Неверный PIN", 1200);
          memset(d_, 0, 4);
          pos_ = 0;
        }
        break;
      case EV_BACK: uiPop(); break;
      default: break;
    }
  }
  void draw() override {
    drawHeader(setMode_ ? "Новый PIN" : "Введите PIN");
    for (int i = 0; i < 4; i++) {
      int x = 18 + i * 25, y = 18;
      if (i == pos_) gfx.drawRBox(x, y, 20, 28, 3);
      else gfx.drawRFrame(x, y, 20, 28, 3);
      gfx.setDrawColor(i == pos_ ? 0 : 1);
      gfx.setFont(F_MID);
      char c[2] = {(char)(i < pos_ ? '*' : '0' + d_[i]), 0};
      if (i < pos_) { gfx.drawDisc(x + 10, y + 14, 3); }
      else gfx.drawStr(x + 5, y + 23, c);
      gfx.setDrawColor(1);
    }
    gfx.setFont(F_TINY);
    drawCenteredUTF8(57, "крутите - цифра, клик - далее");
    if (tries_) {
      char b[24];
      snprintf(b, sizeof(b), "ошибок: %d", tries_);
      drawCenteredUTF8(64, b);
    }
  }
};

static PinEntry pinScreen;
Screen *scrPinEntry() { return &pinScreen; }
void pinEntryStart(void (*onOk)()) {
  pinScreen.start(onOk);
  uiPush(&pinScreen);
}
void pinEntrySetNew() {
  pinScreen.start(nullptr, true);
  uiPush(&pinScreen);
}

// =====================================================================
//  Охрана
// =====================================================================
class SecurityScreen : public Screen {
 public:
  void event(Ev e) override {
    if (e == EV_OK) {
      if (secState() == SEC_DISARMED) {
        secArm();
        if (secState() == SEC_ARMING) toast("Покиньте зону", 1500);
      } else if (cfg.pinEnabled) {
        pinEntryStart([] { secDisarm(); toast("Охрана снята", 1200); });
      } else {
        secDisarm();
        toast("Охрана снята", 1200);
      }
      return;
    }
    if (e == EV_RIGHT || e == EV_NEXT) { uiPush(scrLog()); return; }
    Screen::event(e);
  }
  bool keepAwake() override { return secState() == SEC_ALARM || secState() == SEC_ARMING; }

  void draw() override {
    SecState st = secState();
    bool flash = st == SEC_ALARM && (millis() / 250) % 2;
    drawHeader("Охрана");

    drawArt(Icons::SHIELD, 16, 2, 14, 2);
    if (st == SEC_DISARMED) {  // «открытый» щит — перечёркнут
      gfx.drawLine(4, 44, 32, 16);
    }

    gfx.setFont(F_BOLD);
    gfx.drawUTF8(40, 24, secStateName());
    gfx.setFont(F_SMALL);
    char b[48];
    switch (st) {
      case SEC_DISARMED:
        gfx.drawUTF8(40, 35, "OK - поставить");
        break;
      case SEC_ARMING: {
        uint32_t left = secArmingLeftSec();
        snprintf(b, sizeof(b), "через %lu с", (unsigned long)left);
        gfx.drawUTF8(40, 35, b);
        drawProgress(40, 38, 84, 7, 1.0f - (float)left / max<uint8_t>(1, cfg.armDelaySec));
        break;
      }
      case SEC_ARMED:
        gfx.drawUTF8(40, 35, "OK - снять");
        break;
      case SEC_ALARM:
        snprintf(b, sizeof(b), "причина: %s", secLastTrigger());
        gfx.drawUTF8(40, 35, b);
        break;
    }
    gfx.setFont(F_TINY);
    snprintf(b, sizeof(b), "PIR:%s  Сонар:%s", cfg.alarmUsePir && cfg.pirEnabled ? "да" : "нет",
             cfg.alarmUseSonar ? (S.sonarOk ? "да" : "нет эха") : "нет");
    gfx.drawUTF8(40, 52, b);
    snprintf(b, sizeof(b), "Тревог: %lu   > журнал", (unsigned long)secAlarmCount());
    gfx.drawUTF8(40, 61, b);

    if (flash) {  // мигание всего экрана при тревоге
      gfx.setDrawColor(2);
      gfx.drawBox(0, 0, SCR_W, SCR_H);
      gfx.setDrawColor(1);
    }
  }
};

Screen *scrSecurity() { static SecurityScreen s; return &s; }

// =====================================================================
//  Реле / термостат
// =====================================================================
class RelayScreen : public Screen {
  uint8_t focus_ = 0;  // 0 — режим, 1 — параметр режима

  bool hasParam() const {
    return cfg.relayMode == RELAY_HEAT || cfg.relayMode == RELAY_COOL || cfg.relayMode == RELAY_MOTION;
  }

  void change(int d) {
    if (focus_ == 0) {
      cfg.relayMode = (cfg.relayMode + RELAY_MODE_COUNT + d) % RELAY_MODE_COUNT;
      return;
    }
    switch (cfg.relayMode) {
      case RELAY_HEAT: case RELAY_COOL:
        cfg.thermoTarget = constrain(cfg.thermoTarget + d * 0.5f, -20.0f, 100.0f);
        break;
      case RELAY_MOTION:
        cfg.motionLightSec = constrain((int)cfg.motionLightSec + d * 5, 5, 3600);
        break;
    }
  }

 public:
  void leave() override { settingsSave(); }
  void event(Ev e) override {
    switch (e) {
      case EV_NEXT: case EV_RIGHT: change(+1); break;
      case EV_PREV: change(-1); break;
      case EV_OK:
        if (cfg.relayMode == RELAY_MANUAL) { cfg.relayManualOn = !cfg.relayManualOn; focus_ = 0; }
        else focus_ = hasParam() ? focus_ ^ 1 : 0;
        break;
      default: Screen::event(e);
    }
  }

  void focusBox(int x, int yBase, const char *s, bool f) {
    int w = gfx.getUTF8Width(s);
    if (f) { gfx.drawRBox(x - 2, yBase - 9, w + 4, 11, 2); gfx.setDrawColor(0); }
    gfx.drawUTF8(x, yBase, s);
    gfx.setDrawColor(1);
  }

  void draw() override {
    drawHeader("Реле");
    // лампа состояния
    if (relayIsOn()) { gfx.drawDisc(118, 21, 6); }
    else gfx.drawCircle(118, 21, 6);

    gfx.setFont(F_MAIN);
    gfx.drawUTF8(2, 23, "Режим:");
    focusBox(44, 23, relayModeName(cfg.relayMode), focus_ == 0);

    char b[48];
    switch (cfg.relayMode) {
      case RELAY_OFF:
        gfx.setFont(F_SMALL);
        drawCenteredUTF8(42, "Реле всегда выключено");
        break;
      case RELAY_MANUAL:
        gfx.setFont(F_TITLE);
        drawCenteredUTF8(46, cfg.relayManualOn ? "ВКЛЮЧЕНО" : "ВЫКЛЮЧЕНО");
        gfx.setFont(F_TINY);
        drawCenteredUTF8(60, "OK - переключить");
        break;
      case RELAY_HEAT:
      case RELAY_COOL: {
        float t = tempForThermostat();
        if (isnan(t)) {
          gfx.setFont(F_MAIN);
          gfx.drawUTF8(2, 44, cfg.thermoSource ? "нет DHT11" : "нет DS18B20");
        } else {
          drawTempValue(2, 50, t, F_BIG, true);
        }
        gfx.setFont(F_TINY);
        gfx.drawUTF8(78, 34, "цель:");
        gfx.setFont(F_MAIN);
        fmtFloat(b, sizeof(b), cToUnit(cfg.thermoTarget), 1);
        focusBox(78, 46, b, focus_ == 1);
        gfx.setFont(F_TINY);
        snprintf(b, sizeof(b), "+-%.1f %s", cfg.thermoHyst / 2, cfg.relayMode == RELAY_HEAT ? "нагрев" : "холод");
        drawRightUTF8(SCR_W, 63, b);
        break;
      }
      case RELAY_ALARM:
        gfx.setFont(F_SMALL);
        drawCenteredUTF8(40, "Включится при тревоге");
        drawCenteredUTF8(52, "(сирена, прожектор)");
        break;
      case RELAY_MOTION: {
        gfx.setFont(F_SMALL);
        gfx.drawUTF8(2, 40, "Свет после движения:");
        snprintf(b, sizeof(b), "%u c", cfg.motionLightSec);
        gfx.setFont(F_MAIN);
        focusBox(2, 54, b, focus_ == 1);
        if (relayIsOn() && S.lastMotionMs) {
          uint32_t el = (millis() - S.lastMotionMs) / 1000;
          snprintf(b, sizeof(b), "ещё %lu c", (unsigned long)(cfg.motionLightSec > el ? cfg.motionLightSec - el : 0));
          gfx.setFont(F_TINY);
          drawRightUTF8(SCR_W, 54, b);
        }
        break;
      }
    }
  }
};

Screen *scrRelay() { static RelayScreen s; return &s; }

// =====================================================================
//  Журнал событий
// =====================================================================
class LogScreen : public Screen {
  int top_ = 0;

 public:
  void enter() override { top_ = 0; }
  void event(Ev e) override {
    int n = logCount();
    if (e == EV_NEXT) top_ = min(top_ + 1, max(0, n - 6));
    else if (e == EV_PREV) top_ = max(top_ - 1, 0);
    else Screen::event(e);
  }
  void draw() override {
    drawHeader("Журнал");
    int n = logCount();
    if (!n) { gfx.setFont(F_MAIN); drawCenteredUTF8(38, "Пусто"); return; }
    uint32_t now = millis();
    for (int i = 0; i < 6 && top_ + i < n; i++) {
      const LogEntry &e = logGet(top_ + i);
      uint32_t ago = (now - e.ms) / 1000;
      char t[8];
      if (clockValid()) {
        time_t ts = time(nullptr) - ago;
        struct tm tm;
        localtime_r(&ts, &tm);
        snprintf(t, sizeof(t), "%02d:%02d", tm.tm_hour, tm.tm_min);
      } else if (ago < 60) snprintf(t, sizeof(t), "%luс", (unsigned long)ago);
      else if (ago < 3600) snprintf(t, sizeof(t), "%luм", (unsigned long)(ago / 60));
      else snprintf(t, sizeof(t), "%luч", (unsigned long)(ago / 3600));
      int y = HEADER_H + 8 + i * 9;
      gfx.setFont(F_TINY);
      gfx.drawUTF8(0, y, t);
      gfx.setFont(F_SMALL);
      gfx.drawUTF8(clockValid() ? 22 : 17, y, e.text);
    }
    drawScrollbar(top_, max(1, n - 5));
  }
};

Screen *scrLog() { static LogScreen s; return &s; }
