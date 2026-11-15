#include "display.h"
#include "config.h"
#include "settings.h"
#include "logic.h"
#include "net.h"
#include "sensors.h"

// Для SH1106 1.3" замените на U8G2_SH1106_128X64_NONAME_F_HW_I2C
static U8G2_SSD1306_128X64_NONAME_F_HW_I2C oled(U8G2_R0, U8X8_PIN_NONE, PIN_I2C_SCL, PIN_I2C_SDA);
U8G2 &gfx = oled;

void displayBegin() {
  oled.setBusClock(OLED_I2C_HZ);
  oled.begin();
  oled.enableUTF8Print();
  displayApplySettings();
}

void displayApplySettings() {
  oled.setContrast(cfg.contrast);
  oled.setFlipMode(cfg.flipScreen ? 1 : 0);
}

void displayPower(bool on) { oled.setPowerSave(on ? 0 : 1); }

void px(int x, int y) {
  if (x >= 0 && y >= 0 && x < SCR_W && y < SCR_H) gfx.drawPixel(x, y);
}

void drawArt(const char *const *rows, uint8_t nrows, int x, int y, uint8_t scale) {
  for (uint8_t r = 0; r < nrows; r++) {
    const char *row = rows[r];
    for (uint8_t c = 0; row[c]; c++) {
      if (row[c] != '#') continue;
      int X = x + c * scale, Y = y + r * scale;
      if (scale == 1) { px(X, Y); continue; }
      if (X < 0 || Y < 0 || X + scale > SCR_W || Y + scale > SCR_H) {
        for (uint8_t i = 0; i < scale; i++)
          for (uint8_t j = 0; j < scale; j++) px(X + i, Y + j);
      } else {
        gfx.drawBox(X, Y, scale, scale);
      }
    }
  }
}

void drawHeader(const char *title) {
  gfx.setDrawColor(1);
  gfx.drawBox(0, 0, SCR_W, HEADER_H);
  gfx.setDrawColor(0);
  gfx.setFont(F_SMALL);
  gfx.drawUTF8(2, 8, title);

  int x = SCR_W - 9;
  if (netWifiActive())              { drawArt(Icons::S_WIFI, 7, x, 2); x -= 9; }
  if (secState() == SEC_ALARM)      { if ((millis() / 250) % 2) drawArt(Icons::S_BELL, 7, x, 1); x -= 9; }
  else if (secState() != SEC_DISARMED) { drawArt(Icons::S_LOCK, 7, x, 1); x -= 9; }
  if (relayIsOn())                  { drawArt(Icons::S_PLUG, 7, x, 1); x -= 9; }
  if (S.motion)                     { drawArt(Icons::S_RUN, 7, x, 1); x -= 9; }
  gfx.setDrawColor(1);
}

void drawCenteredUTF8(int y, const char *s) {
  int w = gfx.getUTF8Width(s);
  gfx.drawUTF8((SCR_W - w) / 2, y, s);
}

void drawRightUTF8(int xRight, int y, const char *s) {
  gfx.drawUTF8(xRight - gfx.getUTF8Width(s), y, s);
}

void drawProgress(int x, int y, int w, int h, float frac) {
  frac = constrain(frac, 0.0f, 1.0f);
  gfx.drawFrame(x, y, w, h);
  int fw = (int)((w - 4) * frac + 0.5f);
  if (fw > 0) gfx.drawBox(x + 2, y + 2, fw, h - 4);
}

void drawScrollbar(int pos, int total) {
  if (total <= 1) return;
  const int top = HEADER_H + 2, h = SCR_H - top - 1;
  gfx.drawVLine(SCR_W - 2, top, h);
  int th = max(4, h / total);
  int ty = top + (h - th) * pos / (total - 1);
  gfx.drawBox(SCR_W - 3, ty, 3, th);
}

int drawDegreeUnit(int x, int yBaseline, bool big) {
  if (big) {
    gfx.drawCircle(x + 3, yBaseline - 19, 3);
    gfx.setFont(F_BOLD);
    gfx.drawStr(x + 8, yBaseline - 8, unitStr());
    return 16;
  }
  gfx.drawCircle(x + 2, yBaseline - 7, 1);
  gfx.drawStr(x + 5, yBaseline, unitStr());
  return 5 + gfx.getStrWidth(unitStr());
}

int drawTempValue(int x, int yBaseline, float celsius, const uint8_t *numFont, bool big, uint8_t decimals) {
  char buf[12];
  fmtFloat(buf, sizeof(buf), cToUnit(celsius), decimals);
  gfx.setFont(numFont);
  gfx.drawStr(x, yBaseline, buf);
  int w = gfx.getStrWidth(buf);
  return w + drawDegreeUnit(x + w + 1, yBaseline, big) + 1;
}

void drawSoftKey(const char *left, const char *right) {
  gfx.setFont(F_TINY);
  if (left) gfx.drawUTF8(0, SCR_H - 1, left);
  if (right) drawRightUTF8(SCR_W, SCR_H - 1, right);
}

float cToUnit(float c) { return cfg.fahrenheit ? c * 9.0f / 5.0f + 32.0f : c; }
const char *unitStr() { return cfg.fahrenheit ? "F" : "C"; }

void fmtFloat(char *buf, size_t n, float v, uint8_t decimals, const char *nanStr) {
  if (isnan(v)) { snprintf(buf, n, "%s", nanStr); return; }
  snprintf(buf, n, "%.*f", decimals, v);
}

void fmtDuration(char *buf, size_t n, uint32_t sec) {
  uint32_t d = sec / 86400, h = (sec / 3600) % 24, m = (sec / 60) % 60, s = sec % 60;
  if (d)      snprintf(buf, n, "%luд %02lu:%02lu", (unsigned long)d, (unsigned long)h, (unsigned long)m);
  else if (h) snprintf(buf, n, "%lu:%02lu:%02lu", (unsigned long)h, (unsigned long)m, (unsigned long)s);
  else        snprintf(buf, n, "%02lu:%02lu", (unsigned long)m, (unsigned long)s);
}

// ---------------- Уведомления ----------------
static char toastText[40];
static uint32_t toastUntil = 0;

void toast(const char *msg, uint16_t ms) {
  strncpy(toastText, msg, sizeof(toastText) - 1);
  toastText[sizeof(toastText) - 1] = 0;
  toastUntil = millis() + ms;
}

bool toastVisible() { return toastUntil && (int32_t)(toastUntil - millis()) > 0; }

void drawToast() {
  if (!toastVisible()) return;
  gfx.setFont(F_MAIN);
  int w = gfx.getUTF8Width(toastText) + 12;
  if (w > SCR_W - 4) w = SCR_W - 4;
  int x = (SCR_W - w) / 2, y = 22, h = 20;
  gfx.setDrawColor(0);
  gfx.drawRBox(x - 1, y - 1, w + 2, h + 2, 4);
  gfx.setDrawColor(1);
  gfx.drawRFrame(x, y, w, h, 4);
  gfx.drawRFrame(x + 1, y + 1, w - 2, h - 2, 3);
  drawCenteredUTF8(y + 14, toastText);
}

// ---------------- Иконки ----------------
namespace Icons {
const char *const DASH[16] = {
  "................",
  "................",
  ".....######.....",
  "...##......##...",
  "..#..........#..",
  ".#..#.....##..#.",
  ".#.......##...#.",
  "#.......##.....#",
  "#.#....##....#.#",
  "#......##......#",
  "#.....####.....#",
  "#.....####.....#",
  ".#............#.",
  "..############..",
  "................",
  "................",
};
const char *const GRAPH[16] = {
  "................",
  "#...............",
  "#............#..",
  "#...........#.#.",
  "#..........#...#",
  "#....#....#.....",
  "#...#.#..#......",
  "#..#...##.......",
  "#.#.............",
  "##..............",
  "#...............",
  "#..#..#..#..#...",
  "################",
  "................",
  "................",
  "................",
};
const char *const SONAR[16] = {
  "................",
  "..........#.....",
  "...........#....",
  "......#.....#...",
  ".......#.....#..",
  "...#....#....#..",
  "....#...#.....#.",
  "##..#...#.....#.",
  "##..#...#.....#.",
  "....#...#.....#.",
  "...#....#....#..",
  ".......#.....#..",
  "......#.....#...",
  "...........#....",
  "..........#.....",
  "................",
};
const char *const JOY[16] = {
  "................",
  "......####......",
  ".....######.....",
  ".....######.....",
  "......####......",
  ".......##.......",
  ".......##.......",
  ".......##.......",
  ".......##.......",
  "..############..",
  ".#............#.",
  "#..##......#...#",
  "#..##.....#.#..#",
  "#..........#...#",
  ".#............#.",
  "..############..",
};
const char *const SHIELD[16] = {
  "................",
  ".......##.......",
  ".....##..##.....",
  "...##......##...",
  ".##..........##.",
  ".#............#.",
  ".#.....##.....#.",
  ".#....#..#....#.",
  ".#....#..#....#.",
  ".#...######...#.",
  "..#..######..#..",
  "..#...####...#..",
  "...#........#...",
  "....##....##....",
  "......####......",
  "................",
};
const char *const THERMO[16] = {
  ".......##.......",
  "......#..#......",
  "......#..#.###..",
  "......#..#......",
  "......#..#.##...",
  "......#..#......",
  "......####.###..",
  "......####......",
  "......####.##...",
  "......####......",
  ".....######.....",
  "....########....",
  "....########....",
  "....########....",
  ".....######.....",
  "......####......",
};
const char *const GAME[16] = {
  "................",
  "................",
  "................",
  "...##########...",
  "..#..........#..",
  ".#...#.....#..#.",
  ".#...#....#.#.#.",
  "#..#####...#...#",
  "#....#...#.....#",
  "#....#..#.#....#",
  "#........#.....#",
  "#.....####.....#",
  ".#...#....#...#.",
  "..###......###..",
  "................",
  "................",
};
const char *const TOOLS[16] = {
  "................",
  "..........####..",
  ".........#...#..",
  "........#...#...",
  "........#..#..#.",
  "........#.....#.",
  ".......#.....#..",
  "......#..####...",
  ".....#..#.......",
  "....#..#........",
  "...#..#.........",
  "..#..#..........",
  ".#..#...........",
  ".#.#............",
  "..#.............",
  "................",
};
const char *const WIFI[16] = {
  "................",
  "................",
  "....########....",
  "..##........##..",
  ".#............#.",
  "#....######....#",
  "...##......##...",
  "..#..........#..",
  ".....######.....",
  "....#......#....",
  "................",
  "......####......",
  ".......##.......",
  "................",
  "................",
  "................",
};
const char *const GEAR[16] = {
  "................",
  ".......##.......",
  "...##.####.##...",
  "...##########...",
  "....###..###....",
  "..###......###..",
  "####........####",
  ".###........###.",
  ".###........###.",
  "####........####",
  "..###......###..",
  "....###..###....",
  "...##########...",
  "...##.####.##...",
  ".......##.......",
  "................",
};
const char *const INFO[16] = {
  "................",
  ".....######.....",
  "...##......##...",
  "..#....##....#..",
  ".#.....##.....#.",
  ".#............#.",
  "#.....###......#",
  "#......##......#",
  "#......##......#",
  "#......##......#",
  ".#.....##.....#.",
  ".#....####....#.",
  "..#..........#..",
  "...##......##...",
  ".....######.....",
  "................",
};
const char *const DROP[16] = {
  ".......#........",
  ".......#........",
  "......###.......",
  "......###.......",
  ".....#####......",
  ".....#####......",
  "....##.####.....",
  "....#.#####.....",
  "...##.######....",
  "...#.#######....",
  "...#.#######....",
  "...##.######....",
  "....#######.....",
  ".....#####......",
  "................",
  "................",
};
const char *const PERSON[16] = {
  ".......##.......",
  "......####......",
  "......####......",
  ".......##.......",
  ".....######.....",
  "....#.####.#....",
  "...#..####..#...",
  "......####......",
  "......####......",
  ".....##..##.....",
  ".....#....#.....",
  "....##....##....",
  "....#......#....",
  "...##......##...",
  "................",
  "................",
};
const char *const S_WIFI[7] = {
  ".#####.",
  "#.....#",
  "..###..",
  ".#...#.",
  "...#...",
  "...#...",
  ".......",
};
const char *const S_LOCK[7] = {
  "..###..",
  ".#...#.",
  ".#...#.",
  "#######",
  "###.###",
  "###.###",
  "#######",
};
const char *const S_BELL[7] = {
  "...#...",
  "..###..",
  ".#####.",
  ".#####.",
  "#######",
  ".......",
  "...#...",
};
const char *const S_PLUG[7] = {
  "....##.",
  "...##..",
  "..##...",
  ".#####.",
  "...##..",
  "..##...",
  ".##....",
};
const char *const S_RUN[7] = {
  "...##..",
  "...##..",
  ".####..",
  "#.##.#.",
  "..###..",
  ".#..#..",
  "#....#.",
};
}  // namespace Icons
