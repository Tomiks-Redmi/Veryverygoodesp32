#pragma once
#include <Arduino.h>

// Логические события управления. Энкодер и джойстик дают одни и те же события,
// поэтому всем интерфейсом можно управлять любым из них.
enum Ev : uint8_t {
  EV_NONE = 0,
  EV_PREV,    // энкодер против часовой / джойстик вверх
  EV_NEXT,    // энкодер по часовой    / джойстик вниз
  EV_LEFT,    // джойстик влево
  EV_RIGHT,   // джойстик вправо
  EV_OK,      // клик энкодера / клик джойстика
  EV_BACK,    // долгое нажатие энкодера или джойстика
};

struct JoyState {
  int  x = 0, y = 0;      // -100..100, вправо/вниз = +
  bool btn = false;
  bool present = false;   // джойстик обнаружен при старте
  int  rawX = 0, rawY = 0;
};

extern JoyState joy;

void     inputBegin();
void     inputUpdate();
Ev       inputPoll();               // следующее событие из очереди
void     inputClear();
void     inputPush(Ev e);           // для веб-пульта
void     joyCalibrate();
bool     encButtonDown();
uint32_t inputLastActivity();
void     inputTouch();              // сбросить таймер бездействия
void     inputSetJoyNav(bool on);   // в играх джойстик не генерирует навигацию
