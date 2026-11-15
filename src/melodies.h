#pragma once
#include "sound.h"

struct Melody {
  const char *name;
  const Note *notes;
  uint16_t    len;
};

extern const Melody MELODIES[];
extern const uint8_t MELODY_COUNT;
