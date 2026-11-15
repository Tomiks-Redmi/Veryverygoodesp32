#pragma once
#include <Arduino.h>

enum RelayMode : uint8_t {
  RELAY_OFF = 0,
  RELAY_MANUAL,   // вручную
  RELAY_HEAT,     // термостат: нагрев
  RELAY_COOL,     // термостат: охлаждение
  RELAY_ALARM,    // включается при тревоге
  RELAY_MOTION,   // свет по движению
  RELAY_MODE_COUNT
};

enum LedMode : uint8_t { LED_OFF = 0, LED_HEARTBEAT, LED_MOTION, LED_ALARM_ONLY, LED_MODE_COUNT };

struct Settings {
  uint32_t magic;

  // Экран и интерфейс
  uint8_t  contrast;        // 0..255
  uint8_t  screensaverMin;  // 0 = выкл
  bool     flipScreen;
  bool     encInvert;
  bool     wakeOnMotion;

  // Звук
  bool     sound;
  bool     clickSound;

  // Единицы
  bool     fahrenheit;

  // Модули
  bool     pirEnabled;
  bool     joyEnabled;
  bool     joyInvertX;
  bool     joyInvertY;
  bool     analogEnabled;

  // Оповещения о температуре (°C)
  float    tempHigh;
  float    tempLow;
  bool     tempAlerts;

  // Охрана
  bool     alarmUsePir;
  bool     alarmUseSonar;
  uint16_t alarmDistCm;
  uint8_t  armDelaySec;
  bool     pinEnabled;
  uint16_t pinCode;          // 0000..9999

  // Реле / термостат
  uint8_t  relayMode;
  bool     relayManualOn;
  bool     relayActiveLow;
  float    thermoTarget;
  float    thermoHyst;
  uint8_t  thermoSource;     // 0 = DS18B20, 1 = DHT11
  uint16_t motionLightSec;

  // Светодиод
  uint8_t  ledMode;

  // Графики
  uint8_t  graphStepIdx;     // индекс в GRAPH_STEPS

  // WiFi
  bool     wifiOn;
  char     staSsid[33];
  char     staPass[65];
};

extern Settings cfg;

void settingsLoad();
void settingsSave();
void settingsReset();
void settingsApply();        // применить к железу (контраст, поворот и т.п.)

// Рекорды в играх (хранятся отдельно в NVS)
uint32_t recordGet(const char *key);
void     recordSet(const char *key, uint32_t v);
