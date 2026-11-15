#include "logic.h"
#include "config.h"
#include "settings.h"
#include "sensors.h"
#include "sound.h"
#include "display.h"
#include <stdarg.h>

// ---------------- Журнал ----------------
static const uint8_t LOG_SIZE = 16;
static LogEntry logBuf[LOG_SIZE];
static uint8_t logHead = 0, logN = 0;

void logEvent(const char *fmt, ...) {
  LogEntry &e = logBuf[logHead];
  e.ms = millis();
  va_list ap;
  va_start(ap, fmt);
  vsnprintf(e.text, sizeof(e.text), fmt, ap);
  va_end(ap);
  logHead = (logHead + 1) % LOG_SIZE;
  if (logN < LOG_SIZE) logN++;
  Serial.printf("[%8lu] %s\n", (unsigned long)e.ms, e.text);
}

uint8_t logCount() { return logN; }
const LogEntry &logGet(uint8_t i) { return logBuf[(logHead + LOG_SIZE - 1 - i) % LOG_SIZE]; }

// ---------------- Охрана ----------------
static SecState sec = SEC_DISARMED;
static uint32_t secSince = 0;
static uint32_t alarmCount = 0;
static uint32_t lastMotionSeen = 0;
static char lastTrigger[20] = "-";
static bool sonarWasNear = false;

SecState secState() { return sec; }
uint32_t secAlarmCount() { return alarmCount; }
const char *secLastTrigger() { return lastTrigger; }

const char *secStateName() {
  switch (sec) {
    case SEC_DISARMED: return "Снята";
    case SEC_ARMING:   return "Постановка";
    case SEC_ARMED:    return "На охране";
    case SEC_ALARM:    return "ТРЕВОГА!";
  }
  return "?";
}

uint32_t secArmingLeftSec() {
  if (sec != SEC_ARMING) return 0;
  uint32_t el = (millis() - secSince) / 1000;
  return el >= cfg.armDelaySec ? 0 : cfg.armDelaySec - el;
}

void secArm() {
  if (sec != SEC_DISARMED) return;
  sec = cfg.armDelaySec ? SEC_ARMING : SEC_ARMED;
  secSince = millis();
  lastMotionSeen = S.motionCount;
  sonarWasNear = false;
  logEvent("Постановка на охрану");
  if (sec == SEC_ARMED) sfx(SFX_ARMED);
}

void secDisarm() {
  if (sec == SEC_DISARMED) return;
  sec = SEC_DISARMED;
  soundStop();
  sfx(SFX_DISARMED);
  logEvent("Охрана снята");
}

static void secTrigger(const char *why) {
  strncpy(lastTrigger, why, sizeof(lastTrigger) - 1);
  if (sec == SEC_ALARM) return;
  sec = SEC_ALARM;
  secSince = millis();
  alarmCount++;
  logEvent("ТРЕВОГА: %s", why);
  toast("ТРЕВОГА!", 3000);
}

static void secUpdate(uint32_t now) {
  bool pirHit = cfg.alarmUsePir && cfg.pirEnabled && S.motionCount != lastMotionSeen;
  lastMotionSeen = S.motionCount;
  bool near = cfg.alarmUseSonar && cfg.alarmDistCm && S.sonarOk && S.distCm < cfg.alarmDistCm;
  bool sonarHit = near && !sonarWasNear;
  sonarWasNear = near;

  switch (sec) {
    case SEC_DISARMED:
      break;
    case SEC_ARMING: {
      static uint32_t lastTick = 0;
      if (now - lastTick >= 1000) { lastTick = now; beep(1500, 30); }
      if (now - secSince >= cfg.armDelaySec * 1000UL) {
        sec = SEC_ARMED;
        secSince = now;
        sfx(SFX_ARMED);
        logEvent("Охрана включена");
        sonarWasNear = near;  // не срабатывать на уже стоящий предмет
      }
      break;
    }
    case SEC_ARMED:
      if (pirHit) secTrigger("движение");
      else if (sonarHit) secTrigger("сонар");
      break;
    case SEC_ALARM: {
      // Сирена: плавающий тон
      uint32_t el = now - secSince;
      if (el < ALARM_SIREN_SEC * 1000UL) {
        uint32_t ph = (el % 1200) / 20 * 20;  // шаг 20 мс
        uint16_t f = ph < 600 ? 900 + ph * 2 : 900 + (1200 - ph) * 2;
        soundTone(f);
      } else {
        soundStop();
        sec = SEC_ARMED;  // снова на охране, но счётчик тревог сохранён
        secSince = now;
        logEvent("Сирена отключена (таймаут)");
      }
      if (pirHit) strncpy(lastTrigger, "движение", sizeof(lastTrigger) - 1);
      break;
    }
  }
}

// ---------------- Реле ----------------
static bool relayState = false;
static bool thermoOn = false;

bool relayIsOn() { return relayState; }

void relaySetManual(bool on) {
  cfg.relayManualOn = on;
  if (cfg.relayMode != RELAY_MANUAL) cfg.relayMode = RELAY_MANUAL;
}

const char *relayModeName(uint8_t mode) {
  static const char *names[RELAY_MODE_COUNT] = {"Выкл", "Вручную", "Нагрев", "Охлажд.", "Тревога", "Движение"};
  return mode < RELAY_MODE_COUNT ? names[mode] : "?";
}

static void writeRelay(bool on) {
  if (on != relayState) {
    relayState = on;
    logEvent("Реле: %s", on ? "ВКЛ" : "ВЫКЛ");
  }
  digitalWrite(PIN_RELAY, (on ^ cfg.relayActiveLow) ? HIGH : LOW);
}

static void relayUpdate(uint32_t now) {
  bool on = false;
  switch (cfg.relayMode) {
    case RELAY_OFF: on = false; break;
    case RELAY_MANUAL: on = cfg.relayManualOn; break;
    case RELAY_HEAT:
    case RELAY_COOL: {
      float t = tempForThermostat();
      if (isnan(t)) { thermoOn = false; break; }  // нет датчика — безопасно выключаем
      float lo = cfg.thermoTarget - cfg.thermoHyst / 2, hi = cfg.thermoTarget + cfg.thermoHyst / 2;
      if (cfg.relayMode == RELAY_HEAT) {
        if (t <= lo) thermoOn = true;
        else if (t >= hi) thermoOn = false;
      } else {
        if (t >= hi) thermoOn = true;
        else if (t <= lo) thermoOn = false;
      }
      on = thermoOn;
      break;
    }
    case RELAY_ALARM: on = sec == SEC_ALARM; break;
    case RELAY_MOTION:
      on = S.lastMotionMs && (now - S.lastMotionMs < cfg.motionLightSec * 1000UL);
      break;
  }
  writeRelay(on);
}

// ---------------- Оповещения о температуре ----------------
static int8_t tempZone = 0;  // -1 холодно, 0 норма, 1 жарко

static void tempAlertUpdate() {
  float t = tempMain();
  if (isnan(t) || !cfg.tempAlerts) { tempZone = 0; return; }
  int8_t z = tempZone;
  if (t > cfg.tempHigh) z = 1;
  else if (t < cfg.tempLow) z = -1;
  else if (t < cfg.tempHigh - 0.5f && t > cfg.tempLow + 0.5f) z = 0;
  if (z != tempZone) {
    tempZone = z;
    char b[28];
    if (z == 1)       { snprintf(b, sizeof(b), "Жарко! %.1f", cToUnit(t)); toast(b, 2500); sfx(SFX_ALERT); logEvent("Высокая темп. %.1fC", t); }
    else if (z == -1) { snprintf(b, sizeof(b), "Холодно! %.1f", cToUnit(t)); toast(b, 2500); sfx(SFX_ALERT); logEvent("Низкая темп. %.1fC", t); }
  }
}

// ---------------- Движение ----------------
static uint32_t motionSeen = 0;

static void motionUpdate() {
  if (S.motionCount != motionSeen) {
    motionSeen = S.motionCount;
    if (sec == SEC_DISARMED) logEvent("Движение #%lu", (unsigned long)S.motionCount);
  }
}

// ---------------- Светодиод ----------------
static void ledUpdate(uint32_t now) {
  bool on = false;
  if (sec == SEC_ALARM) on = (now / 100) % 2;
  else if (sec == SEC_ARMING) on = (now / 250) % 2;
  else switch (cfg.ledMode) {
    case LED_HEARTBEAT: { uint32_t p = now % 2000; on = p < 60 || (p > 200 && p < 260); break; }
    case LED_MOTION: on = S.motion; break;
    case LED_ALARM_ONLY: on = sec == SEC_ARMED && (now % 3000) < 40; break;
    default: on = false;
  }
  digitalWrite(PIN_LED, on);
}

void logicBegin() {
  pinMode(PIN_LED, OUTPUT);
  pinMode(PIN_RELAY, OUTPUT);
  relayState = true;   // чтобы writeRelay гарантированно записал вывод
  writeRelay(false);
  logN = 0;
  logHead = 0;
  logEvent("Старт системы");
}

void logicUpdate() {
  uint32_t now = millis();
  motionUpdate();
  secUpdate(now);
  relayUpdate(now);
  tempAlertUpdate();
  ledUpdate(now);
}
