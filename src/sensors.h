#pragma once
#include <Arduino.h>

struct SensorData {
  // DHT11 / DHT22
  float    dhtT = NAN, dhtH = NAN;
  bool     dhtOk = false;
  uint32_t dhtErrors = 0;
  float    dewPoint = NAN;

  // DS18B20 (поддерживается до 4 шт. на одной шине)
  enum { DS_MAX = 4 };
  float    dsT[DS_MAX] = {NAN, NAN, NAN, NAN};
  uint8_t  dsCount = 0;
  bool     dsOk = false;

  // HC-SR04
  float    distCm = NAN;     // сглаженное (медиана из 5)
  float    distRaw = NAN;    // последнее измерение без сглаживания
  uint32_t distSeq = 0;      // счётчик измерений
  bool     sonarOk = false;

  // PIR
  bool     motion = false;
  uint32_t motionCount = 0;
  uint32_t lastMotionMs = 0;

  // Аналоговый вход
  uint8_t  analogPct = 0;
  uint16_t analogRaw = 0;
};

extern SensorData S;

// ------------ История для графиков ------------
enum HistId : uint8_t { H_DS = 0, H_DHT_T, H_DHT_H, H_DIST, H_ANALOG, H_COUNT };
static const int HIST_LEN = 112;

struct History {
  float    v[HIST_LEN];
  uint16_t head = 0, count = 0;
  void  push(float x) { v[head] = x; head = (head + 1) % HIST_LEN; if (count < HIST_LEN) count++; }
  float at(int i) const { return v[(head + HIST_LEN - count + i) % HIST_LEN]; }  // 0 = самое старое
  void  clear() { head = count = 0; }
};

extern History hist[H_COUNT];
extern const uint16_t GRAPH_STEPS_SEC[];
extern const uint8_t  GRAPH_STEPS_COUNT;

void  sensorsBegin();
void  sensorsUpdate();
float tempMain();          // DS18B20, если есть, иначе DHT
float tempForThermostat();
float speedOfSound();      // м/с с учётом температуры
