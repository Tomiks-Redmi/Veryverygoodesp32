// Понг, рисовалка, музыкальная шкатулка
#include "screens.h"
#include "display.h"
#include "settings.h"
#include "sound.h"
#include "input.h"
#include "melodies.h"

// =====================================================================
//  Понг: вы справа (энкодер или джойстик), компьютер слева
// =====================================================================
class Pong : public Screen {
  static const int TOP = 11, BOT = 63, PH = 14, WIN = 7;
  float by_ = 37, bx_ = 64, vx_ = 2, vy_ = 1;
  float py_ = 30, ay_ = 30;  // верх ракеток
  int sp_ = 0, sa_ = 0;
  uint8_t st_ = 0;  // 0 старт, 1 игра, 2 пауза, 3 конец
  uint32_t serveAt_ = 0;

  void serve(int dir) {
    bx_ = 64; by_ = (TOP + BOT) / 2;
    vx_ = 2.0f * dir;
    vy_ = (random(0, 2) ? 1 : -1) * (0.6f + random(0, 10) / 10.0f);
    serveAt_ = millis() + 700;
  }

 public:
  bool gamepad() override { return true; }
  bool keepAwake() override { return st_ == 1; }
  void enter() override { st_ = 0; sp_ = sa_ = 0; serve(1); }
  void event(Ev e) override {
    if (e == EV_BACK) { uiPop(); return; }
    if (e == EV_OK) {
      if (st_ == 1) st_ = 2;
      else { if (st_ != 2) { sp_ = sa_ = 0; serve(1); } st_ = 1; sfx(SFX_START); }
      return;
    }
    if (st_ != 1) return;
    if (e == EV_NEXT) py_ += 5;
    if (e == EV_PREV) py_ -= 5;
  }
  void update() override {
    if (st_ != 1) return;
    py_ += joy.y * 0.04f;
    py_ = constrain(py_, (float)TOP, (float)(BOT - PH));
    // ИИ с ограниченной скоростью и «ленью»
    float target = (vx_ < 0 ? by_ : (TOP + BOT) / 2.0f) - PH / 2.0f;
    float aiSpeed = 1.3f + (sp_ + sa_) * 0.08f;
    ay_ += constrain(target - ay_, -aiSpeed, aiSpeed);
    ay_ = constrain(ay_, (float)TOP, (float)(BOT - PH));

    if ((int32_t)(millis() - serveAt_) < 0) return;
    bx_ += vx_; by_ += vy_;
    if (by_ <= TOP) { by_ = TOP; vy_ = -vy_; }
    if (by_ >= BOT - 2) { by_ = BOT - 2; vy_ = -vy_; }

    // ракетка игрока (x = 124)
    if (vx_ > 0 && bx_ >= 122 && bx_ <= 126 && by_ + 2 >= py_ && by_ <= py_ + PH) {
      float k = (by_ + 1 - (py_ + PH / 2.0f)) / (PH / 2.0f);  // -1..1
      vx_ = -min(fabsf(vx_) * 1.06f, 5.0f);
      vy_ = k * 2.2f;
      bx_ = 121;
      beep(1800, 15);
    }
    // ракетка ИИ (x = 2)
    if (vx_ < 0 && bx_ <= 5 && bx_ >= 1 && by_ + 2 >= ay_ && by_ <= ay_ + PH) {
      float k = (by_ + 1 - (ay_ + PH / 2.0f)) / (PH / 2.0f);
      vx_ = min(fabsf(vx_) * 1.06f, 5.0f);
      vy_ = k * 2.2f;
      bx_ = 6;
      beep(1500, 15);
    }
    if (bx_ < -4) { sp_++; sfx(SFX_COIN); serve(-1); }
    if (bx_ > 132) { sa_++; sfx(SFX_ERROR); serve(1); }
    if (sp_ >= WIN || sa_ >= WIN) { st_ = 3; sfx(sp_ > sa_ ? SFX_WIN : SFX_GAMEOVER); }
  }
  void draw() override {
    char b[40];
    snprintf(b, sizeof(b), "Понг   ИИ %d : %d Вы", sa_, sp_);
    drawHeader(b);
    for (int y = TOP; y < BOT; y += 4) gfx.drawVLine(64, y, 2);
    gfx.drawBox(1, (int)ay_, 2, PH);
    gfx.drawBox(125, (int)py_, 2, PH);
    if (bx_ >= 0 && bx_ < 127) gfx.drawBox((int)bx_, (int)by_, 2, 2);
    if (st_ != 1) {
      gfx.setDrawColor(0);
      gfx.drawRBox(20, 22, 88, 30, 3);
      gfx.setDrawColor(1);
      gfx.drawRFrame(20, 22, 88, 30, 3);
      gfx.setFont(F_BOLD);
      drawCenteredUTF8(36, st_ == 0 ? "ПОНГ" : st_ == 2 ? "Пауза" : (sp_ > sa_ ? "Победа!" : "Поражение"));
      gfx.setFont(F_TINY);
      drawCenteredUTF8(46, st_ == 0 ? "до 7 очков, OK - старт" : "OK - продолжить");
    }
  }
};

Screen *scrPong() { static Pong s; return &s; }

// =====================================================================
//  Рисовалка: джойстик — курсор, клик джойстика — перо вверх/вниз
// =====================================================================
class DrawPad : public Screen {
  uint8_t canvas_[128 * 64 / 8];  // формат XBM
  float cx_ = 64, cy_ = 32;
  bool pen_ = true;
  uint8_t brush_ = 0;  // 0 — 1px, 1 — 3px, 2 — ластик
  uint32_t clearArm_ = 0;
  bool lastJoyBtn_ = false;
  bool ignoreOk_ = false;  // EV_OK от отпускания джойстика — не очистка

  void plot(int x, int y, bool on) {
    if (x < 0 || y < 0 || x >= 128 || y >= 64) return;
    uint8_t &byte = canvas_[y * 16 + x / 8];
    if (on) byte |= 1 << (x & 7); else byte &= ~(1 << (x & 7));
  }
  void stamp(int x, int y) {
    if (brush_ == 0) { plot(x, y, true); return; }
    int r = brush_ == 1 ? 1 : 2;
    for (int dx = -r; dx <= r; dx++)
      for (int dy = -r; dy <= r; dy++) plot(x + dx, y + dy, brush_ == 1);
  }

 public:
  bool gamepad() override { return true; }
  bool keepAwake() override { return true; }
  void enter() override { memset(canvas_, 0, sizeof(canvas_)); toast("Рисуйте джойстиком!", 1500); }
  void event(Ev e) override {
    switch (e) {
      case EV_BACK: uiPop(); break;
      case EV_NEXT: case EV_PREV: {
        brush_ = (brush_ + (e == EV_NEXT ? 1 : 2)) % 3;
        static const char *names[3] = {"Кисть: тонкая", "Кисть: толстая", "Ластик"};
        toast(names[brush_], 700);
        break;
      }
      case EV_OK:
        if (ignoreOk_) { ignoreOk_ = false; break; }  // клик джойстика обработан в update()
        if (clearArm_ && millis() - clearArm_ < 2000) { memset(canvas_, 0, sizeof(canvas_)); clearArm_ = 0; toast("Очищено", 600); }
        else { clearArm_ = millis(); toast("Ещё клик - очистить", 1200); }
        break;
      default: break;
    }
  }
  void update() override {
    // клик джойстика переключает перо
    if (joy.btn && !lastJoyBtn_) { pen_ = !pen_; ignoreOk_ = true; }
    lastJoyBtn_ = joy.btn;
    float nx = constrain(cx_ + joy.x * 0.03f, 0.0f, 127.0f);
    float ny = constrain(cy_ + joy.y * 0.03f, 0.0f, 63.0f);
    if (pen_) {
      // линия от старой точки к новой, без пропусков
      int steps = max(1, (int)max(fabsf(nx - cx_), fabsf(ny - cy_)));
      for (int i = 1; i <= steps; i++) stamp((int)(cx_ + (nx - cx_) * i / steps), (int)(cy_ + (ny - cy_) * i / steps));
    }
    cx_ = nx; cy_ = ny;
  }
  void draw() override {
    if (!joy.present || !cfg.joyEnabled) {
      drawHeader("Рисовалка");
      gfx.setFont(F_MAIN);
      drawCenteredUTF8(36, "Нужен джойстик");
      return;
    }
    gfx.drawXBM(0, 0, 128, 64, canvas_);
    // курсор: крестик, мигает если перо поднято
    int x = (int)cx_, y = (int)cy_;
    if (pen_ || (millis() / 250) % 2) {
      gfx.setDrawColor(2);
      for (int i = 2; i <= 4; i++) { px(x - i, y); px(x + i, y); px(x, y - i); px(x, y + i); }
      gfx.setDrawColor(1);
    }
  }
};

Screen *scrDraw() { static DrawPad s; return &s; }

// =====================================================================
//  Музыкальная шкатулка
// =====================================================================
class MusicBox : public Screen {
  int sel_ = 0;
  uint8_t bars_[16] = {0};

 public:
  bool keepAwake() override { return soundMelodyPlaying(); }
  void leave() override { soundStop(); }
  void event(Ev e) override {
    switch (e) {
      case EV_NEXT: case EV_RIGHT: sel_ = (sel_ + 1) % MELODY_COUNT; soundStop(); break;
      case EV_PREV: sel_ = (sel_ + MELODY_COUNT - 1) % MELODY_COUNT; soundStop(); break;
      case EV_OK:
        if (soundMelodyPlaying()) soundStop();
        else soundMelody(MELODIES[sel_].notes, MELODIES[sel_].len, false, true);
        break;
      default: Screen::event(e);
    }
  }
  void update() override {
    uint16_t f = soundCurrentFreq();
    for (int i = 0; i < 16; i++) bars_[i] = bars_[i] > 2 ? bars_[i] - 2 : 0;
    if (f) {
      int band = constrain((int)(log2f(f / 400.0f) * 6), 0, 15);
      for (int i = -2; i <= 2; i++) {
        int k = band + i;
        if (k >= 0 && k < 16) bars_[k] = max<int>(bars_[k], 26 - abs(i) * 7 + random(0, 4));
      }
    }
  }
  void draw() override {
    char b[32];
    snprintf(b, sizeof(b), "Музыка %d/%d", sel_ + 1, MELODY_COUNT);
    drawHeader(b);
    gfx.setFont(F_MAIN);
    drawCenteredUTF8(23, MELODIES[sel_].name);
    // эквалайзер
    for (int i = 0; i < 16; i++) {
      int h = bars_[i];
      if (h) gfx.drawBox(4 + i * 7 + 1, 52 - h, 5, h);
      else gfx.drawHLine(4 + i * 7 + 1, 51, 5);
    }
    bool playing = soundMelodyPlaying();
    if (playing) drawProgress(4, 54, 120, 4, (float)soundMelodyPos() / MELODIES[sel_].len);
    gfx.setFont(F_TINY);
    drawCenteredUTF8(64, playing ? "OK - стоп" : "OK - играть, крутить - выбор");
  }
};

Screen *scrMusic() { static MusicBox s; return &s; }
