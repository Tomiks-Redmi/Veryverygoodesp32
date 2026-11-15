#include "sound.h"
#include "config.h"
#include "settings.h"

static const uint8_t LEDC_CH = 0;

struct Note { uint16_t f, ms; };
static Note seq[24];
static uint8_t seqLen = 0, seqPos = 0;
static uint32_t noteEnd = 0;
static bool continuous = false;
static uint16_t curFreq = 0xFFFF;

static void hwTone(uint16_t f) {
  if (f == curFreq) return;  // не перенастраиваем таймер без нужды
  curFreq = f;
  ledcWriteTone(LEDC_CH, f);
}

void soundBegin() {
  ledcSetup(LEDC_CH, 2000, 10);
  ledcAttachPin(PIN_BUZZER, LEDC_CH);
  hwTone(0);
}

static void play(const Note *n, uint8_t len, bool force = false) {
  if (!cfg.sound && !force) return;
  if (len > sizeof(seq) / sizeof(seq[0])) len = sizeof(seq) / sizeof(seq[0]);
  memcpy(seq, n, len * sizeof(Note));
  seqLen = len;
  seqPos = 0;
  continuous = false;
  hwTone(seq[0].f);
  noteEnd = millis() + seq[0].ms;
}

void beep(uint16_t freq, uint16_t ms, bool force) {
  Note n = {freq, ms};
  play(&n, 1, force);
}

void soundTone(uint16_t freq) {
  seqLen = 0;
  continuous = freq != 0;
  hwTone(freq);
}

void soundStop() { seqLen = 0; continuous = false; hwTone(0); }
bool soundBusy() { return seqLen > 0 || continuous; }

void soundUpdate() {
  if (!seqLen || (int32_t)(millis() - noteEnd) < 0) return;
  seqPos++;
  if (seqPos >= seqLen) { seqLen = 0; hwTone(0); return; }
  hwTone(seq[seqPos].f);
  noteEnd = millis() + seq[seqPos].ms;
}

// Ноты
enum : uint16_t { N_C5 = 523, N_D5 = 587, N_E5 = 659, N_F5 = 698, N_G5 = 784, N_A5 = 880, N_B5 = 988,
                  N_C6 = 1047, N_E6 = 1319, N_G6 = 1568, N_C7 = 2093 };

void sfx(Sfx s) {
  if (continuous) return;  // не перебиваем сирену/терменвокс
  switch (s) {
    case SFX_CLICK:
      if (cfg.clickSound) beep(3200, 4);
      break;
    case SFX_OK:      { static const Note n[] = {{N_C6, 30}, {N_G6, 40}}; play(n, 2); } break;
    case SFX_BACK:    { static const Note n[] = {{N_G6, 30}, {N_C6, 40}}; play(n, 2); } break;
    case SFX_ERROR:   { static const Note n[] = {{300, 120}, {0, 40}, {250, 180}}; play(n, 3); } break;
    case SFX_START:   { static const Note n[] = {{N_C5, 80}, {N_E5, 80}, {N_G5, 80}, {N_C6, 160}}; play(n, 4); } break;
    case SFX_ARMED:   { static const Note n[] = {{N_A5, 100}, {0, 50}, {N_A5, 100}, {0, 50}, {N_E6, 250}}; play(n, 5, true); } break;
    case SFX_DISARMED:{ static const Note n[] = {{N_E6, 100}, {N_C6, 100}, {N_A5, 200}}; play(n, 3, true); } break;
    case SFX_ALERT:   { static const Note n[] = {{2400, 90}, {0, 60}, {2400, 90}, {0, 60}, {2400, 90}}; play(n, 5); } break;
    case SFX_EAT:     { static const Note n[] = {{N_E6, 25}, {N_G6, 35}}; play(n, 2); } break;
    case SFX_COIN:    { static const Note n[] = {{N_B5, 50}, {N_E6, 120}}; play(n, 2); } break;
    case SFX_GAMEOVER:{ static const Note n[] = {{N_G5, 150}, {N_E5, 150}, {N_C5, 150}, {200, 400}}; play(n, 4); } break;
    case SFX_WIN:     { static const Note n[] = {{N_C6, 90}, {N_E6, 90}, {N_G6, 90}, {N_C7, 250}}; play(n, 4); } break;
    case SFX_MOTION:  { static const Note n[] = {{1800, 30}, {0, 30}, {1800, 30}}; play(n, 3); } break;
  }
}
