#pragma once
#include <Arduino.h>

// ---------- Охрана ----------
enum SecState : uint8_t { SEC_DISARMED = 0, SEC_ARMING, SEC_ARMED, SEC_ALARM };

SecState    secState();
const char *secStateName();
uint32_t    secArmingLeftSec();
uint32_t    secAlarmCount();
void        secArm();
void        secDisarm();
const char *secLastTrigger();

// ---------- Реле ----------
bool relayIsOn();
void relaySetManual(bool on);
const char *relayModeName(uint8_t mode);

// ---------- Журнал событий ----------
struct LogEntry {
  uint32_t ms;
  char     text[28];
};
void            logEvent(const char *fmt, ...);
uint8_t         logCount();
const LogEntry &logGet(uint8_t i);  // 0 = самое новое

// ---------- Общее ----------
void logicBegin();
void logicUpdate();
