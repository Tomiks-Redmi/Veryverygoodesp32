#include "settings.h"
#include <Preferences.h>
#include "display.h"

// Меняйте при изменении структуры Settings — настройки сбросятся к умолчаниям
static const uint32_t SETTINGS_MAGIC = 0x56470103;

Settings cfg;
static Preferences prefs;

static void setDefaults() {
  memset(&cfg, 0, sizeof(cfg));
  cfg.magic          = SETTINGS_MAGIC;
  cfg.contrast       = 200;
  cfg.screensaverMin = 3;
  cfg.flipScreen     = false;
  cfg.encInvert      = false;
  cfg.wakeOnMotion   = true;
  cfg.sound          = true;
  cfg.clickSound     = true;
  cfg.fahrenheit     = false;
  cfg.pirEnabled     = true;
  cfg.joyEnabled     = true;
  cfg.joyInvertX     = false;
  cfg.joyInvertY     = false;
  cfg.analogEnabled  = false;
  cfg.tempHigh       = 30.0f;
  cfg.tempLow        = 10.0f;
  cfg.tempAlerts     = true;
  cfg.alarmUsePir    = true;
  cfg.alarmUseSonar  = true;
  cfg.alarmDistCm    = 50;
  cfg.armDelaySec    = 10;
  cfg.pinEnabled     = false;
  cfg.pinCode        = 1234;
  cfg.relayMode      = RELAY_OFF;
  cfg.relayManualOn  = false;
  cfg.relayActiveLow = false;
  cfg.thermoTarget   = 24.0f;
  cfg.thermoHyst     = 0.5f;
  cfg.thermoSource   = 0;
  cfg.motionLightSec = 30;
  cfg.ledMode        = LED_HEARTBEAT;
  cfg.graphStepIdx   = 1;
  cfg.wifiOn         = true;
}

void settingsLoad() {
  setDefaults();
  if (!prefs.begin("verygood", true)) return;   // namespace ещё не создан
  Settings tmp;
  size_t n = prefs.getBytes("cfg", &tmp, sizeof(tmp));
  prefs.end();
  if (n == sizeof(tmp) && tmp.magic == SETTINGS_MAGIC) {
    cfg = tmp;
    cfg.staSsid[sizeof(cfg.staSsid) - 1] = 0;
    cfg.staPass[sizeof(cfg.staPass) - 1] = 0;
  }
}

void settingsSave() {
  cfg.magic = SETTINGS_MAGIC;
  if (prefs.begin("verygood", false)) {
    prefs.putBytes("cfg", &cfg, sizeof(cfg));
    prefs.end();
  }
}

void settingsReset() {
  setDefaults();
  settingsSave();
  settingsApply();
}

void settingsApply() {
  displayApplySettings();
}

uint32_t recordGet(const char *key) {
  uint32_t v = 0;
  if (prefs.begin("records", true)) {
    v = prefs.getUInt(key, 0);
    prefs.end();
  }
  return v;
}

void recordSet(const char *key, uint32_t v) {
  if (prefs.begin("records", false)) {
    prefs.putUInt(key, v);
    prefs.end();
  }
}
