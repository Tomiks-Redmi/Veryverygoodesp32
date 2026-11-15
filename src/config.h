// =====================================================================
//  VeryGood ESP32 — конфигурация железа
//  Все пины и константы собраны здесь. Поменяйте под свою схему.
// =====================================================================
#pragma once

#define FW_NAME     "VeryGood ESP32"
#define FW_VERSION  "1.0.0"

// ---------- OLED 0.96" SSD1306 128x64 (I2C) ----------
#define PIN_I2C_SDA      21
#define PIN_I2C_SCL      22
#define OLED_I2C_HZ      400000
// Если у вас SH1106 (1.3"), поменяйте класс дисплея в display.cpp

// ---------- Энкодер KY-040 ----------
#define PIN_ENC_A        32   // CLK
#define PIN_ENC_B        33   // DT
#define PIN_ENC_BTN      25   // SW
#define ENC_STEPS_PER_DETENT 4  // у большинства KY-040 = 4, у некоторых = 2

// ---------- Датчики температуры ----------
#define PIN_DHT          4    // DHT11 DATA (нужен подтягивающий 10к, на модуле обычно есть)
#define DHT_TYPE         DHT11 // DHT22 тоже подойдёт
#define PIN_DS18B20      5    // DS18B20 DQ (подтяжка 4.7к к 3.3В обязательна!)

// ---------- Ультразвуковой сонар HC-SR04 ----------
#define PIN_SONAR_TRIG   26
#define PIN_SONAR_ECHO   27   // ECHO у HC-SR04 5-вольтовый -> делитель 1к/2к!
#define SONAR_MAX_CM     400

// ---------- Датчик движения HC-SR501 / AM312 ----------
#define PIN_PIR          14

// ---------- Аналоговый джойстик KY-023 ----------
#define PIN_JOY_X        34   // только ADC1 пины, т.к. ADC2 не работает с WiFi
#define PIN_JOY_Y        35
#define PIN_JOY_BTN      13

// ---------- Дополнительно ----------
#define PIN_ANALOG       36   // VP: фоторезистор / потенциометр / датчик влажности почвы
#define PIN_BUZZER       18   // пассивный пьезо-бузер
#define PIN_LED          2    // встроенный светодиод
#define PIN_RELAY        19   // модуль реле

// ---------- WiFi ----------
#define WIFI_AP_SSID     "VeryGood-ESP32"
#define WIFI_AP_PASS     "esp32pass"   // минимум 8 символов
#define MDNS_NAME        "verygood"     // http://verygood.local

// ---------- Тайминги ----------
#define DHT_PERIOD_MS        2500
#define DS_PERIOD_MS         1000
#define SONAR_PERIOD_MS      70
#define UI_FRAME_MS          33     // ~30 FPS
#define ALARM_SIREN_SEC      60
#define LONG_PRESS_MS        600
