// Часы, будильник
#include "screens.h"
#include "display.h"
#include "settings.h"
#include "sensors.h"
#include "sound.h"
#include "clock.h"
#include "melodies.h"
#include "net.h"

// Обрезать UTF-8 строку до maxChars символов
static void utf8Trunc(char *dst, size_t n, const char *src, int maxChars) {
  size_t i = 0;
  int chars = 0;
  while (src[i] && i + 1 < n) {
    if (((uint8_t)src[i] & 0xC0) != 0x80) {
      if (chars == maxChars) break;
      chars++;
    }
    dst[i] = src[i];
    i++;
  }
  dst[i] = 0;
}

// =====================================================================
//  Часы
// =====================================================================
class ClockScreen : public Screen {
 public:
  void event(Ev e) override {
    if (e == EV_OK || e == EV_RIGHT) { uiPush(scrAlarmEdit()); return; }
    Screen::event(e);
  }
  bool keepAwake() override { return false; }
  void draw() override {
    struct tm t;
    if (!clockNow(t)) {
      drawHeader("Часы");
      gfx.setFont(F_MAIN);
      drawCenteredUTF8(28, "Время не задано");
      gfx.setFont(F_TINY);
      drawCenteredUTF8(40, "подключите WiFi-роутер");
      drawCenteredUTF8(48, "или откройте веб-панель");
      drawCenteredUTF8(56, "с телефона - время возьмётся");
      drawCenteredUTF8(64, "OK - будильник");
      return;
    }
    char b[32];
    snprintf(b, sizeof(b), "%s, %d %s", clockWeekday(t.tm_wday, true), t.tm_mday, clockMonthGen(t.tm_mon));
    drawHeader(b);

    // ЧЧ : ММ с мигающим двоеточием
    gfx.setFont(F_HUGE);
    char hh[4], mm[4];
    snprintf(hh, sizeof(hh), "%02d", t.tm_hour);
    snprintf(mm, sizeof(mm), "%02d", t.tm_min);
    int wh = gfx.getStrWidth(hh), wm = gfx.getStrWidth(mm);
    int x = (SCR_W - wh - wm - 10) / 2;
    gfx.drawStr(x, 47, hh);
    if (t.tm_sec % 2 == 0) {
      gfx.drawBox(x + wh + 3, 24, 4, 4);
      gfx.drawBox(x + wh + 3, 38, 4, 4);
    }
    gfx.drawStr(x + wh + 10, 47, mm);

    // секунды — тонкая полоска
    int sw = t.tm_sec * 120 / 59;
    gfx.drawHLine(4, 51, sw);
    gfx.drawPixel(4 + 120, 51);

    // температура и будильник
    float tc = tempMain();
    if (!isnan(tc)) { drawTempValue(2, 63, tc, F_SMALL, false); }
    if (cfg.alarmClockOn) {
      snprintf(b, sizeof(b), "%02d:%02d", cfg.alarmHour, cfg.alarmMin);
      gfx.setFont(F_SMALL);
      int w = gfx.getStrWidth(b);
      gfx.drawStr(SCR_W - w, 63, b);
      drawArt(Icons::S_BELL, 7, SCR_W - w - 9, 56);
    }
  }
};

Screen *scrClock() { static ClockScreen s; return &s; }

// =====================================================================
//  Звонок будильника
// =====================================================================
class AlarmRing : public Screen {
 public:
  bool keepAwake() override { return true; }
  void event(Ev e) override {
    if (e == EV_OK || e == EV_BACK) { clockAlarmStop(); uiPop(); }
    else if (e == EV_NEXT || e == EV_PREV || e == EV_LEFT || e == EV_RIGHT) { clockAlarmSnooze(); uiPop(); }
  }
  void update() override {
    if (!clockAlarmRinging()) uiPop();
  }
  void draw() override {
    bool ph = (millis() / 300) % 2;
    // качающийся колокольчик
    int dx = ph ? -2 : 2;
    drawArt(Icons::S_BELL, 7, 56 + dx, 4, 2);
    char b[8];
    clockFmtHM(b, sizeof(b));
    gfx.setFont(F_BIG);
    int w = gfx.getStrWidth(b);
    gfx.drawStr((SCR_W - w) / 2, 46, b);
    gfx.setFont(F_TINY);
    drawCenteredUTF8(56, "OK - выключить");
    drawCenteredUTF8(64, "покрутить - ещё 5 минут");
    if (ph) { gfx.drawFrame(0, 0, SCR_W, SCR_H); gfx.drawFrame(1, 1, SCR_W - 2, SCR_H - 2); }
  }
};

Screen *scrAlarmRing() { static AlarmRing s; return &s; }

// =====================================================================
//  Настройка будильника и времени
// =====================================================================
static const uint8_t DAY_PRESETS[] = {0x7F, 0x1F, 0x60};
static const char *const DAY_NAMES[] = {"каждый день", "будни", "выходные"};

static int dayPresetIdx() {
  for (int i = 0; i < 3; i++) if (cfg.alarmDays == DAY_PRESETS[i]) return i;
  return 0;
}

static const EdRow ALARM_ROWS[] = {
  {"Будильник",
   [](char *b, size_t n) { snprintf(b, n, "%s", cfg.alarmClockOn ? "вкл" : "выкл"); },
   [](int) { cfg.alarmClockOn = !cfg.alarmClockOn; }, true},
  {"Час",
   [](char *b, size_t n) { snprintf(b, n, "%02d", cfg.alarmHour); },
   [](int d) { cfg.alarmHour = (cfg.alarmHour + 24 + d) % 24; }, false},
  {"Минуты",
   [](char *b, size_t n) { snprintf(b, n, "%02d", cfg.alarmMin); },
   [](int d) { cfg.alarmMin = (cfg.alarmMin + 60 + d) % 60; }, false},
  {"Дни",
   [](char *b, size_t n) { snprintf(b, n, "%s", DAY_NAMES[dayPresetIdx()]); },
   [](int d) { cfg.alarmDays = DAY_PRESETS[(dayPresetIdx() + 3 + d) % 3]; }, false},
  {"Мелодия",
   [](char *b, size_t n) { utf8Trunc(b, n, MELODIES[cfg.alarmMelody % MELODY_COUNT].name, 9); },
   [](int d) {
     cfg.alarmMelody = (cfg.alarmMelody + MELODY_COUNT + d) % MELODY_COUNT;
     const Melody &m = MELODIES[cfg.alarmMelody];
     soundMelody(m.notes, min<uint16_t>(m.len, 8), false, true);  // прослушать начало
   }, false},
  {"Пояс",
   [](char *b, size_t n) { snprintf(b, n, "UTC%+d", cfg.tzOffset); },
   [](int d) { cfg.tzOffset = constrain(cfg.tzOffset + d, -12, 14); clockApplyTz(); }, false},
  {"Синхр. время",
   [](char *b, size_t n) { snprintf(b, n, "%s", clockSource()); },
   [](int) {
     if (netStaConnected()) { clockStartNtp(); toast("Синхронизация...", 1200); }
     else toast("Нет интернета", 1200);
   }, true},
};

class AlarmEdit : public RowEditor {
 public:
  AlarmEdit() : RowEditor("Будильник", ALARM_ROWS, sizeof(ALARM_ROWS) / sizeof(ALARM_ROWS[0])) {}
  void leave() override { soundStop(); RowEditor::leave(); }
};

Screen *scrAlarmEdit() { static AlarmEdit s; return &s; }
