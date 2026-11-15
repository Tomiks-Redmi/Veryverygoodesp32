#pragma once
#include <Arduino.h>

enum Sfx : uint8_t {
  SFX_CLICK, SFX_OK, SFX_BACK, SFX_ERROR, SFX_START, SFX_ARMED, SFX_DISARMED,
  SFX_ALERT, SFX_EAT, SFX_GAMEOVER, SFX_COIN, SFX_WIN, SFX_MOTION
};

struct Note { uint16_t f, ms; };   // f = 0 — пауза

void soundBegin();
void soundUpdate();
void sfx(Sfx s);
void beep(uint16_t freq, uint16_t ms, bool force = false);
void soundTone(uint16_t freq);   // непрерывный тон (0 = тишина), для сирены и терменвокса
void soundStop();
bool soundBusy();

// Длинные мелодии (массив не копируется — должен жить всё время игры)
void     soundMelody(const Note *notes, uint16_t len, bool loop = false, bool force = false);
bool     soundMelodyPlaying();
uint16_t soundMelodyPos();
uint16_t soundCurrentFreq();
