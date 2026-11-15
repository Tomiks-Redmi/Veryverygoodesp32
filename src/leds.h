#pragma once
#include <Arduino.h>

// Адресная RGB-лента WS2812B и сервопривод
void        ledsBegin();
void        ledsUpdate();
void        ledsApply();             // применить количество/яркость из настроек
const char *ledsModeName(uint8_t m);
void        ledsFlash(uint32_t rgb, uint16_t ms);  // короткая вспышка (события)

void servoWrite(int angle);          // 0..180, при первом вызове подключает выход
void servoRelease();                 // отключить сигнал (серво не дёргается)
int  servoAngle();
