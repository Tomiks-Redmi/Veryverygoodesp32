#pragma once
#include <Arduino.h>

// Адресная RGB-лента WS2812B
void        ledsBegin();
void        ledsUpdate();
void        ledsApply();             // применить количество/яркость из настроек
const char *ledsModeName(uint8_t m);
void        ledsFlash(uint32_t rgb, uint16_t ms);  // короткая вспышка (события)
