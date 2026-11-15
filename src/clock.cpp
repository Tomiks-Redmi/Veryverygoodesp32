#include "clock.h"
#include "config.h"
#include "settings.h"
#include "sound.h"
#include "melodies.h"
#include "logic.h"
#include "display.h"
#include <sys/time.h>

static bool ntpStarted = false;
static uint8_t source = 0;  // 0 нет, 1 NTP, 2 телефон

static void buildTz(char *b, size_t n) {
  // В POSIX-формате знак обратный: UTC+3 записывается как "<+03>-3"
  int o = cfg.tzOffset;
  snprintf(b, n, "<%+03d>%d", o, -o);
}

void clockApplyTz() {
  char tz[16];
  buildTz(tz, sizeof(tz));
  setenv("TZ", tz, 1);
  tzset();
}

void clockBegin() { clockApplyTz(); }

void clockStartNtp() {
  char tz[16];
  buildTz(tz, sizeof(tz));
  configTzTime(tz, NTP_SERVER1, NTP_SERVER2);
  ntpStarted = true;
}

bool clockValid() { return time(nullptr) > 1700000000; }

void clockSetEpoch(uint32_t epoch, bool fromNtp) {
  struct timeval tv;
  tv.tv_sec = epoch;
  tv.tv_usec = 0;
  settimeofday(&tv, nullptr);
  source = fromNtp ? 1 : 2;
  clockApplyTz();
}

const char *clockSource() {
  if (!clockValid()) return "нет";
  if (ntpStarted && source != 2) return "интернет";
  return "телефон";
}

bool clockNow(struct tm &t) {
  if (!clockValid()) return false;
  time_t now = time(nullptr);
  localtime_r(&now, &t);
  return true;
}

void clockFmtHM(char *b, size_t n) {
  struct tm t;
  if (clockNow(t)) snprintf(b, n, "%02d:%02d", t.tm_hour, t.tm_min);
  else snprintf(b, n, "--:--");
}

const char *clockWeekday(int wday, bool shortName) {
  static const char *full[7] = {"Воскресенье", "Понедельник", "Вторник", "Среда", "Четверг", "Пятница", "Суббота"};
  static const char *sh[7] = {"Вс", "Пн", "Вт", "Ср", "Чт", "Пт", "Сб"};
  wday = ((wday % 7) + 7) % 7;
  return shortName ? sh[wday] : full[wday];
}

const char *clockMonthGen(int mon) {
  static const char *m[12] = {"января", "февраля", "марта", "апреля", "мая", "июня",
                              "июля", "августа", "сентября", "октября", "ноября", "декабря"};
  return m[((mon % 12) + 12) % 12];
}

// ---------------- Будильник ----------------
static bool ringing = false;
static uint32_t ringSince = 0;
static int lastKey = -1;          // день*1440 + минута последнего срабатывания
static time_t snoozeUntil = 0;

bool clockAlarmRinging() { return ringing; }

static void startRing() {
  ringing = true;
  ringSince = millis();
  uint8_t m = cfg.alarmMelody < MELODY_COUNT ? cfg.alarmMelody : 0;
  soundMelody(MELODIES[m].notes, MELODIES[m].len, true, true);
  logEvent("Будильник!");
}

void clockAlarmStop() {
  if (!ringing) return;
  ringing = false;
  snoozeUntil = 0;
  soundStop();
}

void clockAlarmSnooze() {
  if (!ringing) return;
  ringing = false;
  soundStop();
  snoozeUntil = time(nullptr) + 5 * 60;
  toast("Отложено на 5 мин", 1500);
}

void clockUpdate() {
  static uint32_t lastCheck = 0;
  uint32_t ms = millis();
  if (ms - lastCheck < 500) return;
  lastCheck = ms;

  if (ringing) {
    if (ms - ringSince > 5UL * 60 * 1000) clockAlarmStop();  // само замолкает через 5 минут
    return;
  }
  if (snoozeUntil && time(nullptr) >= snoozeUntil) { snoozeUntil = 0; startRing(); return; }

  struct tm t;
  if (!cfg.alarmClockOn || !clockNow(t)) return;
  int key = t.tm_yday * 1440 + t.tm_hour * 60 + t.tm_min;
  if (key == lastKey) return;
  uint8_t dayBit = 1 << ((t.tm_wday + 6) % 7);  // Пн = бит 0
  if (t.tm_hour == cfg.alarmHour && t.tm_min == cfg.alarmMin && (cfg.alarmDays & dayBit)) {
    lastKey = key;
    startRing();
  }
}
