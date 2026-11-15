#pragma once
#include <Arduino.h>
#include <U8g2lib.h>

extern U8G2 &gfx;

// Шрифты с кириллицей (UTF-8, рисовать через drawUTF8)
#define F_TINY   u8g2_font_4x6_t_cyrillic
#define F_SMALL  u8g2_font_5x7_t_cyrillic
#define F_MAIN   u8g2_font_6x12_t_cyrillic
#define F_BOLD   u8g2_font_6x13B_t_cyrillic
#define F_TITLE  u8g2_font_9x15_t_cyrillic
// Крупные цифры (только 0-9 . - : /)
#define F_MID    u8g2_font_logisoso16_tn
#define F_BIG    u8g2_font_logisoso22_tn
#define F_HUGE   u8g2_font_logisoso32_tn

static const int SCR_W = 128;
static const int SCR_H = 64;
static const int HEADER_H = 10;

void displayBegin();
void displayApplySettings();
void displayPower(bool on);

// Рисование «ASCII-арт» иконок: '#' — пиксель, всё остальное — пусто
void drawArt(const char *const *rows, uint8_t nrows, int x, int y, uint8_t scale = 1);
void px(int x, int y);  // пиксель с проверкой границ

void drawHeader(const char *title);
void drawCenteredUTF8(int y, const char *s);
void drawRightUTF8(int xRight, int y, const char *s);
void drawProgress(int x, int y, int w, int h, float frac);
void drawScrollbar(int pos, int total);
int  drawDegreeUnit(int x, int yBaseline, bool big);  // рисует "°C"/"°F", возвращает ширину
int  drawTempValue(int x, int yBaseline, float celsius, const uint8_t *numFont, bool big, uint8_t decimals = 1);
void drawSoftKey(const char *left, const char *right);

// Форматирование
float cToUnit(float c);       // в выбранные единицы
const char *unitStr();        // "C" / "F"
void  fmtFloat(char *buf, size_t n, float v, uint8_t decimals, const char *nanStr = "--");
void  fmtDuration(char *buf, size_t n, uint32_t sec);

// Всплывающие уведомления поверх любого экрана
void toast(const char *msg, uint16_t ms = 1800);
void drawToast();
bool toastVisible();

// Иконки (16x16)
namespace Icons {
extern const char *const DASH[16];
extern const char *const GRAPH[16];
extern const char *const SONAR[16];
extern const char *const JOY[16];
extern const char *const SHIELD[16];
extern const char *const THERMO[16];
extern const char *const GAME[16];
extern const char *const TOOLS[16];
extern const char *const WIFI[16];
extern const char *const GEAR[16];
extern const char *const INFO[16];
extern const char *const DROP[16];
extern const char *const PERSON[16];
extern const char *const CLOCK[16];
// мелкие 7x7 для строки состояния
extern const char *const S_WIFI[7];
extern const char *const S_LOCK[7];
extern const char *const S_BELL[7];
extern const char *const S_PLUG[7];
extern const char *const S_RUN[7];
}  // namespace Icons
