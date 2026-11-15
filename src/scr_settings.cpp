// Настройки: универсальный редактируемый список
#include "screens.h"
#include "display.h"
#include "settings.h"
#include "sensors.h"
#include "sound.h"
#include "net.h"
#include "logic.h"
#include "clock.h"

enum SType : uint8_t { T_BOOL, T_U8, T_U16, T_FLOAT, T_TEMP, T_ENUM, T_ACTION, T_I8 };

struct SItem {
  const char        *label;
  SType              type;
  void              *ptr;
  float              mn, mx, st;
  const char *const *labels;     // для T_ENUM; для чисел labels[0] — текст для нуля
  void (*cb)();
  const char        *suffix;
};

static const char *const L_UNITS[]  = {"C", "F"};
static const char *const L_STEPS[]  = {"1с", "5с", "30с", "1м", "5м"};
static const char *const L_THSRC[]  = {"DS18B20", "DHT11"};
static const char *const L_LED[]    = {"выкл", "пульс", "движ.", "охрана"};
static const char *const L_OFF[]    = {"выкл"};

static void cbDisplay() { displayApplySettings(); }
static void cbTz() { clockApplyTz(); }
static void cbWifi() { netApply(); }
static void cbForgetSta() {
  cfg.staSsid[0] = 0;
  cfg.staPass[0] = 0;
  settingsSave();
  toast("Роутер забыт", 1200);
}
static void cbNewPin();
static void cbReset();
static void cbReboot() { settingsSave(); toast("Перезагрузка...", 2000); gfx.clearBuffer(); drawToast(); gfx.sendBuffer(); delay(600); ESP.restart(); }

static const SItem ITEMS[] = {
  // label             type     ptr                  min   max   step  labels   cb          suffix
  {"Яркость",          T_U8,    &cfg.contrast,       5,    255,  25,   nullptr, cbDisplay,  nullptr},
  {"Заставка",         T_U8,    &cfg.screensaverMin, 0,    30,   1,    L_OFF,   nullptr,    "м"},
  {"Перевернуть",      T_BOOL,  &cfg.flipScreen,     0,    0,    0,    nullptr, cbDisplay,  nullptr},
  {"Звук",             T_BOOL,  &cfg.sound,          0,    0,    0,    nullptr, nullptr,    nullptr},
  {"Щелчки",           T_BOOL,  &cfg.clickSound,     0,    0,    0,    nullptr, nullptr,    nullptr},
  {"Единицы",          T_ENUM,  &cfg.fahrenheit,     0,    1,    1,    L_UNITS, nullptr,    nullptr},
  {"Шаг графика",      T_ENUM,  &cfg.graphStepIdx,   0,    4,    1,    L_STEPS, nullptr,    nullptr},
  {"Оповещ. темп.",    T_BOOL,  &cfg.tempAlerts,     0,    0,    0,    nullptr, nullptr,    nullptr},
  {"Жарко выше",       T_TEMP,  &cfg.tempHigh,       -20,  80,   0.5,  nullptr, nullptr,    nullptr},
  {"Холодно ниже",     T_TEMP,  &cfg.tempLow,        -30,  60,   0.5,  nullptr, nullptr,    nullptr},
  {"Охрана: PIR",      T_BOOL,  &cfg.alarmUsePir,    0,    0,    0,    nullptr, nullptr,    nullptr},
  {"Охрана: сонар",    T_BOOL,  &cfg.alarmUseSonar,  0,    0,    0,    nullptr, nullptr,    nullptr},
  {"Дист. тревоги",    T_U16,   &cfg.alarmDistCm,    0,    400,  5,    L_OFF,   nullptr,    "см"},
  {"Задержка",         T_U8,    &cfg.armDelaySec,    0,    60,   1,    L_OFF,   nullptr,    "с"},
  {"PIN для снятия",   T_BOOL,  &cfg.pinEnabled,     0,    0,    0,    nullptr, nullptr,    nullptr},
  {"Новый PIN",        T_ACTION, nullptr,            0,    0,    0,    nullptr, cbNewPin,   nullptr},
  {"Гистерезис",       T_FLOAT, &cfg.thermoHyst,     0.1,  5,    0.1,  nullptr, nullptr,    nullptr},
  {"Датчик терм.",     T_ENUM,  &cfg.thermoSource,   0,    1,    1,    L_THSRC, nullptr,    nullptr},
  {"Реле инверсия",    T_BOOL,  &cfg.relayActiveLow, 0,    0,    0,    nullptr, nullptr,    nullptr},
  {"Светодиод",        T_ENUM,  &cfg.ledMode,        0,    3,    1,    L_LED,   nullptr,    nullptr},
  {"PIR датчик",       T_BOOL,  &cfg.pirEnabled,     0,    0,    0,    nullptr, nullptr,    nullptr},
  {"Будить движ.",     T_BOOL,  &cfg.wakeOnMotion,   0,    0,    0,    nullptr, nullptr,    nullptr},
  {"Джойстик",         T_BOOL,  &cfg.joyEnabled,     0,    0,    0,    nullptr, nullptr,    nullptr},
  {"Джойст. инв. X",   T_BOOL,  &cfg.joyInvertX,     0,    0,    0,    nullptr, nullptr,    nullptr},
  {"Джойст. инв. Y",   T_BOOL,  &cfg.joyInvertY,     0,    0,    0,    nullptr, nullptr,    nullptr},
  {"Энкодер инв.",     T_BOOL,  &cfg.encInvert,      0,    0,    0,    nullptr, nullptr,    nullptr},
  {"Аналог. вход",     T_BOOL,  &cfg.analogEnabled,  0,    0,    0,    nullptr, nullptr,    nullptr},
  {"Часовой пояс",     T_I8,    &cfg.tzOffset,       -12,  14,   1,    nullptr, cbTz,       nullptr},
  {"Серво-радар",      T_BOOL,  &cfg.servoRadar,     0,    0,    0,    nullptr, nullptr,    nullptr},
  {"Telegram",         T_BOOL,  &cfg.tgOn,           0,    0,    0,    nullptr, nullptr,    nullptr},
  {"TG: движение",     T_BOOL,  &cfg.tgMotion,       0,    0,    0,    nullptr, nullptr,    nullptr},
  {"WiFi",             T_BOOL,  &cfg.wifiOn,         0,    0,    0,    nullptr, cbWifi,     nullptr},
  {"Забыть роутер",    T_ACTION, nullptr,            0,    0,    0,    nullptr, cbForgetSta, nullptr},
  {"Сброс настроек",   T_ACTION, nullptr,            0,    0,    0,    nullptr, cbReset,    nullptr},
  {"Перезагрузка",     T_ACTION, nullptr,            0,    0,    0,    nullptr, cbReboot,   nullptr},
};
static const int N_ITEMS = sizeof(ITEMS) / sizeof(ITEMS[0]);

static float getNum(const SItem &it) {
  switch (it.type) {
    case T_U8: case T_ENUM: return *(uint8_t *)it.ptr;
    case T_U16:             return *(uint16_t *)it.ptr;
    case T_I8:              return *(int8_t *)it.ptr;
    case T_FLOAT: case T_TEMP: return *(float *)it.ptr;
    default: return 0;
  }
}

static void setNum(const SItem &it, float v) {
  v = constrain(v, it.mn, it.mx);
  switch (it.type) {
    case T_U8: case T_ENUM: *(uint8_t *)it.ptr = (uint8_t)lroundf(v); break;
    case T_U16:             *(uint16_t *)it.ptr = (uint16_t)lroundf(v); break;
    case T_I8:              *(int8_t *)it.ptr = (int8_t)lroundf(v); break;
    case T_FLOAT: case T_TEMP: *(float *)it.ptr = roundf(v * 10) / 10; break;
    default: break;
  }
}

static void fmtValue(const SItem &it, char *b, size_t n) {
  switch (it.type) {
    case T_BOOL:   snprintf(b, n, "%s", *(bool *)it.ptr ? "вкл" : "выкл"); break;
    case T_ACTION: snprintf(b, n, ">"); break;
    case T_ENUM:   snprintf(b, n, "%s", it.labels[*(uint8_t *)it.ptr]); break;
    case T_TEMP:   snprintf(b, n, "%.1f%s", cToUnit(*(float *)it.ptr), unitStr()); break;
    case T_FLOAT:  snprintf(b, n, "%.1f", *(float *)it.ptr); break;
    case T_I8:     snprintf(b, n, "UTC%+d", *(int8_t *)it.ptr); break;
    default: {
      long v = lroundf(getNum(it));
      if (v == 0 && it.labels) snprintf(b, n, "%s", it.labels[0]);
      else snprintf(b, n, "%ld%s", v, it.suffix ? it.suffix : "");
    }
  }
}

class SettingsScreen : public Screen {
  int  sel_ = 0, top_ = 0;
  bool edit_ = false;

 public:
  bool resetArmed = false;

  void enter() override { edit_ = false; resetArmed = false; }
  void leave() override { settingsSave(); }

  void event(Ev e) override {
    const SItem &it = ITEMS[sel_];
    if (edit_) {
      switch (e) {
        case EV_NEXT: case EV_RIGHT: setNum(it, getNum(it) + it.st); if (it.cb) it.cb(); break;
        case EV_PREV: case EV_LEFT:  setNum(it, getNum(it) - it.st); if (it.cb) it.cb(); break;
        case EV_OK: case EV_BACK:    edit_ = false; sfx(SFX_OK); break;
        default: break;
      }
      return;
    }
    switch (e) {
      case EV_NEXT: listMoveSel(sel_, top_, N_ITEMS, 1); resetArmed = false; break;
      case EV_PREV: listMoveSel(sel_, top_, N_ITEMS, -1); resetArmed = false; break;
      case EV_OK: case EV_RIGHT:
        if (it.type == T_BOOL) { *(bool *)it.ptr = !*(bool *)it.ptr; if (it.cb) it.cb(); sfx(SFX_OK); }
        else if (it.type == T_ACTION) { if (it.cb) it.cb(); }
        else { edit_ = true; sfx(SFX_OK); }
        break;
      default: Screen::event(e);
    }
  }

  void draw() override {
    drawHeader(edit_ ? "Настройки: правка" : "Настройки");
    listDrawCursor(sel_ - top_);
    char v[20];
    for (int i = 0; i < LIST_ROWS && top_ + i < N_ITEMS; i++) {
      const SItem &it = ITEMS[top_ + i];
      fmtValue(it, v, sizeof(v));
      if (it.type == T_ACTION && top_ + i == sel_ && resetArmed && it.cb == cbReset) snprintf(v, sizeof(v), "точно?");
      listDrawRow(i, it.label, v, top_ + i == sel_, edit_);
    }
    drawScrollbar(sel_, N_ITEMS);
  }
};

static SettingsScreen settingsScreen;
Screen *scrSettings() { return &settingsScreen; }

static void cbReset() {
  if (!settingsScreen.resetArmed) { settingsScreen.resetArmed = true; return; }
  settingsScreen.resetArmed = false;
  settingsReset();
  netApply();
  toast("Настройки сброшены", 1500);
}

static void cbNewPin() {
  if (cfg.pinEnabled) pinEntryStart([] { pinEntrySetNew(); });
  else pinEntrySetNew();
}
