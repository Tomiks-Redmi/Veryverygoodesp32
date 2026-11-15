#pragma once
#include <Arduino.h>

enum Sfx : uint8_t {
  SFX_CLICK, SFX_OK, SFX_BACK, SFX_ERROR, SFX_START, SFX_ARMED, SFX_DISARMED,
  SFX_ALERT, SFX_EAT, SFX_GAMEOVER, SFX_COIN, SFX_WIN, SFX_MOTION
};

void soundBegin();
void soundUpdate();
void sfx(Sfx s);
void beep(uint16_t freq, uint16_t ms, bool force = false);
void soundTone(uint16_t freq);   // непрерывный тон (0 = тишина), для сирены и терменвокса
void soundStop();
bool soundBusy();
