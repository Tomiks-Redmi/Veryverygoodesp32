// Мелодии для музыкальной шкатулки и будильника (общественное достояние)
#include "melodies.h"

// Частоты нот (Гц)
#define C5 523
#define CS5 554
#define D5 587
#define DS5 622
#define E5 659
#define F5 698
#define FS5 740
#define G5 784
#define GS5 831
#define A5_ 880
#define AS5 932
#define B5 988
#define C6 1047
#define D6 1175
#define DS6 1245
#define E6 1319
#define F6 1397
#define G6 1568
#define R 0

// Длительности
#define Q 300   // четверть
#define E 150   // восьмая
#define H 600   // половина
#define QD 450  // четверть с точкой
#define W 1200  // целая

// Бетховен — «Ода к радости»
static const Note ODE[] = {
  {E6,Q},{E6,Q},{F6,Q},{G6,Q}, {G6,Q},{F6,Q},{E6,Q},{D6,Q},
  {C6,Q},{C6,Q},{D6,Q},{E6,Q}, {E6,QD},{D6,E},{D6,H},
  {E6,Q},{E6,Q},{F6,Q},{G6,Q}, {G6,Q},{F6,Q},{E6,Q},{D6,Q},
  {C6,Q},{C6,Q},{D6,Q},{E6,Q}, {D6,QD},{C6,E},{C6,H}, {R,Q},
};

// Бетховен — «К Элизе» (начало)
static const Note ELISE[] = {
  {E6,E},{DS6,E},{E6,E},{DS6,E},{E6,E},{B5,E},{D6,E},{C6,E},{A5_,Q},{R,E},
  {C5,E},{E5,E},{A5_,E},{B5,Q},{R,E},
  {E5,E},{GS5,E},{B5,E},{C6,Q},{R,E},
  {E5,E},{E6,E},{DS6,E},{E6,E},{DS6,E},{E6,E},{B5,E},{D6,E},{C6,E},{A5_,Q},{R,E},
  {C5,E},{E5,E},{A5_,E},{B5,Q},{R,E},
  {E5,E},{C6,E},{B5,E},{A5_,H}, {R,Q},
};

// Jingle Bells (припев)
static const Note JINGLE[] = {
  {E6,Q},{E6,Q},{E6,H}, {E6,Q},{E6,Q},{E6,H},
  {E6,Q},{G6,Q},{C6,QD},{D6,E}, {E6,W},
  {F6,Q},{F6,Q},{F6,QD},{F6,E}, {F6,Q},{E6,Q},{E6,Q},{E6,E},{E6,E},
  {E6,Q},{D6,Q},{D6,Q},{E6,Q}, {D6,H},{G6,H},
  {E6,Q},{E6,Q},{E6,H}, {E6,Q},{E6,Q},{E6,H},
  {E6,Q},{G6,Q},{C6,QD},{D6,E}, {E6,W},
  {F6,Q},{F6,Q},{F6,QD},{F6,E}, {F6,Q},{E6,Q},{E6,Q},{E6,E},{E6,E},
  {G6,Q},{G6,Q},{F6,Q},{D6,Q}, {C6,W}, {R,Q},
};

// «В лесу родилась ёлочка» (Л. Бекман)
static const Note ELKA[] = {
  {C5,Q},{A5_,Q},{A5_,Q},{G5,Q},{A5_,Q},{F5,Q},{C5,Q},{C5,Q},
  {C5,Q},{A5_,Q},{A5_,Q},{AS5,Q},{G5,Q},{C6,H},{R,Q},
  {C6,Q},{D5,Q},{D5,Q},{AS5,Q},{AS5,Q},{A5_,Q},{G5,Q},{F5,Q},
  {C6,Q},{A5_,Q},{A5_,Q},{G5,Q},{A5_,Q},{F5,H},{R,Q},
};

// Будильник — бодрый сигнал
static const Note WAKE[] = {
  {C6,E},{E6,E},{G6,E},{C6,E},{E6,E},{G6,E},{E6,Q},{R,E},
  {D6,E},{F6,E},{G6,E},{D6,E},{F6,E},{G6,E},{F6,Q},{R,Q},
};

#define M(name, arr) {name, arr, (uint16_t)(sizeof(arr) / sizeof(arr[0]))}
const Melody MELODIES[] = {
  M("Ода к радости", ODE),
  M("К Элизе", ELISE),
  M("Jingle Bells", JINGLE),
  M("Ёлочка", ELKA),
  M("Будильник", WAKE),
};
const uint8_t MELODY_COUNT = sizeof(MELODIES) / sizeof(MELODIES[0]);
