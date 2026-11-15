// Рулетка, счётчик проходов, RGB-подсветка
#include "screens.h"
#include "display.h"
#include "settings.h"
#include "sensors.h"
#include "sound.h"
#include "leds.h"
#include "input.h"

// =====================================================================
//  Рулетка: замер A, B, C -> площадь и объём
// =====================================================================
class Rangefinder : public Screen {
  float v_[3] = {NAN, NAN, NAN};
  uint8_t next_ = 0;

  static void fmtLen(char *b, size_t n, float cm) {
    if (isnan(cm)) snprintf(b, n, "--");
    else if (cm < 100) snprintf(b, n, "%.1fсм", cm);
    else snprintf(b, n, "%.2fм", cm / 100);
  }

 public:
  bool keepAwake() override { return true; }
  void event(Ev e) override {
    if (e == EV_OK) {
      if (!S.sonarOk) { sfx(SFX_ERROR); toast("Нет эха", 800); return; }
      if (next_ >= 3) { v_[0] = v_[1] = v_[2] = NAN; next_ = 0; }
      v_[next_++] = S.distCm;
      beep(2400, 40);
      return;
    }
    if (e == EV_NEXT || e == EV_PREV) { v_[0] = v_[1] = v_[2] = NAN; next_ = 0; toast("Сброс", 600); return; }
    Screen::event(e);
  }
  void draw() override {
    drawHeader("Рулетка");
    char b[48];
    // текущее значение крупно
    gfx.setFont(F_MID);
    if (S.sonarOk) snprintf(b, sizeof(b), S.distCm < 100 ? "%.1f" : "%.0f", S.distCm);
    else snprintf(b, sizeof(b), "--");
    gfx.drawStr(2, 34, b);
    int w = gfx.getStrWidth(b);
    gfx.setFont(F_MAIN);
    gfx.drawUTF8(4 + w, 34, "см");
    // ячейки A B C
    const char *names[3] = {"A", "B", "C"};
    gfx.setFont(F_SMALL);
    for (int i = 0; i < 3; i++) {
      int y = 18 + i * 9;
      char v[16];
      fmtLen(v, sizeof(v), v_[i]);
      snprintf(b, sizeof(b), "%s %s", names[i], v);
      if (i == next_ && next_ < 3 && (millis() / 400) % 2) gfx.drawFrame(76, y - 7, 52, 9);
      gfx.drawUTF8(78, y, b);
    }
    // площадь и объём
    gfx.setFont(F_TINY);
    if (!isnan(v_[1])) {
      float area = v_[0] * v_[1] / 10000.0f;
      if (!isnan(v_[2])) snprintf(b, sizeof(b), "S=%.2f кв.м  V=%.3f куб.м", area, area * v_[2] / 100.0f);
      else snprintf(b, sizeof(b), "S = %.2f кв.м", area);
      gfx.drawUTF8(2, 54, b);
    }
    drawCenteredUTF8(63, next_ < 3 ? "OK - замер, крутить - сброс" : "OK - заново");
  }
};

Screen *scrRangefinder() { static Rangefinder s; return &s; }

// =====================================================================
//  Счётчик проходов: сонар смотрит поперёк двери/коридора
// =====================================================================
class PeopleCounter : public Screen {
  float base_ = NAN;
  bool present_ = false;
  uint32_t presentSince_ = 0, lastPass_ = 0, startMs_ = 0, count_ = 0;
  bool counted_ = false;
  bool sound_ = true;

 public:
  bool keepAwake() override { return true; }
  void enter() override { base_ = NAN; present_ = false; startMs_ = millis(); }
  void event(Ev e) override {
    if (e == EV_OK) { count_ = 0; startMs_ = millis(); base_ = NAN; toast("Сброс и калибровка", 900); return; }
    if (e == EV_NEXT || e == EV_PREV) { sound_ = !sound_; toast(sound_ ? "Звук вкл" : "Звук выкл", 700); return; }
    Screen::event(e);
  }
  void update() override {
    if (!S.sonarOk) return;
    float d = S.distCm;
    if (isnan(base_)) base_ = d;
    float thr = base_ - max(20.0f, base_ * 0.25f);   // кто-то ближе, чем «пустой» фон
    bool p = d < thr;
    uint32_t now = millis();
    if (p && !present_) { presentSince_ = now; counted_ = false; }
    if (p && !counted_ && now - presentSince_ > 80) {
      counted_ = true;
      count_++;
      lastPass_ = now;
      if (sound_) beep(2600, 30);
    }
    present_ = p;
    if (!p) base_ += (d - base_) * 0.02f;  // фон медленно подстраивается
  }
  void draw() override {
    drawHeader("Счётчик проходов");
    if (!S.sonarOk) { gfx.setFont(F_MAIN); drawCenteredUTF8(38, "Нет эха сонара"); return; }
    char b[32];
    snprintf(b, sizeof(b), "%lu", (unsigned long)count_);
    gfx.setFont(F_HUGE);
    gfx.drawStr(4, 46, b);
    gfx.setFont(F_TINY);
    uint32_t el = (millis() - startMs_) / 1000;
    float perHour = el > 30 ? count_ * 3600.0f / el : NAN;
    char d[16];
    fmtFloat(d, sizeof(d), perHour, 0);
    snprintf(b, sizeof(b), "в час: %s", d);
    drawRightUTF8(SCR_W, 20, b);
    snprintf(b, sizeof(b), "фон: %.0f см", base_);
    drawRightUTF8(SCR_W, 28, b);
    snprintf(b, sizeof(b), "сейчас: %.0f см", S.distCm);
    drawRightUTF8(SCR_W, 36, b);
    if (lastPass_) {
      fmtDuration(d, sizeof(d), (millis() - lastPass_) / 1000);
      snprintf(b, sizeof(b), "посл.: %s", d);
      drawRightUTF8(SCR_W, 44, b);
    }
    // индикатор присутствия
    if (present_) gfx.drawBox(0, 52, SCR_W, 3); else gfx.drawHLine(0, 53, SCR_W);
    drawCenteredUTF8(63, "OK - сброс, крутить - звук");
  }
};

Screen *scrPeople() { static PeopleCounter s; return &s; }

// =====================================================================
//  RGB-подсветка WS2812
// =====================================================================
static const EdRow LEDS_ROWS[] = {
  {"Режим",
   [](char *b, size_t n) { snprintf(b, n, "%s", ledsModeName(cfg.ledsMode)); },
   [](int d) { cfg.ledsMode = (cfg.ledsMode + LEDS_MODE_COUNT + d) % LEDS_MODE_COUNT; }, false},
  {"Яркость",
   [](char *b, size_t n) { snprintf(b, n, "%d%%", cfg.ledsBright * 100 / 255); },
   [](int d) { cfg.ledsBright = constrain((int)cfg.ledsBright + d * 15, 5, 255); ledsApply(); }, false},
  {"Светодиодов",
   [](char *b, size_t n) { if (cfg.ledsCount) snprintf(b, n, "%d", cfg.ledsCount); else snprintf(b, n, "нет"); },
   [](int d) { cfg.ledsCount = constrain((int)cfg.ledsCount + d, 0, 64); ledsApply(); }, false},
  {"Тест: вспышка",
   nullptr,
   [](int) { ledsFlash(0xFFFFFF, 400); }, true},
};

class LedsScreen : public RowEditor {
 public:
  LedsScreen() : RowEditor("RGB-подсветка", LEDS_ROWS, sizeof(LEDS_ROWS) / sizeof(LEDS_ROWS[0])) {}
};

Screen *scrLeds() { static LedsScreen s; return &s; }
