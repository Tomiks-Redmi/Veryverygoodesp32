#pragma once
#include <Arduino.h>
#include <time.h>

void        clockBegin();
void        clockApplyTz();          // применить часовой пояс из настроек
void        clockStartNtp();         // запустить синхронизацию (когда есть интернет)
void        clockSetEpoch(uint32_t epoch, bool fromNtp = false);
bool        clockValid();
bool        clockNow(struct tm &t);
void        clockFmtHM(char *b, size_t n);
const char *clockWeekday(int wday, bool shortName);
const char *clockMonthGen(int mon);  // «января», «февраля»...
const char *clockSource();           // откуда время: NTP / телефон / нет

// Будильник
void clockUpdate();
bool clockAlarmRinging();
void clockAlarmStop();
void clockAlarmSnooze();
