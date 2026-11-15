// Игры: змейка, ловец звёзд, тест реакции, терменвокс
#include "screens.h"
#include "display.h"
#include "sensors.h"
#include "settings.h"
#include "sound.h"
#include "input.h"

enum GameState : uint8_t { GS_READY, GS_PLAY, GS_PAUSE, GS_OVER };

static void drawGameOverlay(const char *title, const char *line1, const char *line2) {
  gfx.setDrawColor(0);
  gfx.drawRBox(14, 14, 100, 42, 4);
  gfx.setDrawColor(1);
  gfx.drawRFrame(14, 14, 100, 42, 4);
  gfx.setFont(F_BOLD);
  drawCenteredUTF8(28, title);
  gfx.setFont(F_SMALL);
  if (line1) drawCenteredUTF8(40, line1);
  if (line2) drawCenteredUTF8(50, line2);
}

// Направление джойстика: -1 нет, 0 вверх, 1 вправо, 2 вниз, 3 влево
static int joyDir4() {
  if (abs(joy.x) < 50 && abs(joy.y) < 50) return -1;
  if (abs(joy.x) > abs(joy.y)) return joy.x > 0 ? 1 : 3;
  return joy.y > 0 ? 2 : 0;
}

// =====================================================================
//  Змейка. Энкодер: поворот налево/направо. Джойстик: направление.
// =====================================================================
class Snake : public Screen {
  static const int CELL = 4, COLS = 32, ROWS = 13, OY = 11, MAXLEN = COLS * ROWS;
  uint8_t   bx_[MAXLEN], by_[MAXLEN];
  int       head_ = 0, len_ = 0;
  int       dir_ = 1, nextDir_ = 1;
  int       fx_ = 0, fy_ = 0;
  uint32_t  stepAt_ = 0, score_ = 0, best_ = 0;
  GameState st_ = GS_READY;

  bool occupied(int x, int y) {
    for (int i = 0; i < len_; i++) {
      int k = (head_ - i + MAXLEN) % MAXLEN;
      if (bx_[k] == x && by_[k] == y) return true;
    }
    return false;
  }
  void placeFood() {
    do { fx_ = random(COLS); fy_ = random(ROWS); } while (occupied(fx_, fy_));
  }
  void reset() {
    len_ = 4; head_ = 3; dir_ = nextDir_ = 1; score_ = 0;
    for (int i = 0; i < 4; i++) { bx_[i] = 8 + i; by_[i] = ROWS / 2; }
    placeFood();
  }
  void turn(int d) {  // не даём развернуться на 180°
    if ((d + 2) % 4 != dir_) nextDir_ = d;
  }

 public:
  bool gamepad() override { return true; }
  bool keepAwake() override { return st_ == GS_PLAY; }
  void enter() override { best_ = recordGet("snake"); reset(); st_ = GS_READY; }
  void event(Ev e) override {
    if (e == EV_BACK) { uiPop(); return; }
    if (e == EV_OK) {
      if (st_ == GS_PLAY) st_ = GS_PAUSE;
      else { if (st_ == GS_OVER || st_ == GS_READY) reset(); st_ = GS_PLAY; sfx(SFX_START); }
      return;
    }
    if (st_ != GS_PLAY) return;
    if (e == EV_NEXT) turn((dir_ + 1) % 4);
    if (e == EV_PREV) turn((dir_ + 3) % 4);
  }
  void update() override {
    if (st_ != GS_PLAY) return;
    int jd = joyDir4();
    if (jd >= 0) turn(jd);
    uint32_t speed = max<int>(60, 170 - (int)score_ * 4);
    if (millis() - stepAt_ < speed) return;
    stepAt_ = millis();
    dir_ = nextDir_;
    int x = bx_[head_], y = by_[head_];
    static const int8_t DX[4] = {0, 1, 0, -1}, DY[4] = {-1, 0, 1, 0};
    x = (x + DX[dir_] + COLS) % COLS;   // сквозь стены
    y = (y + DY[dir_] + ROWS) % ROWS;
    bool eat = x == fx_ && y == fy_;
    if (!eat) len_--;                   // хвост уходит
    if (occupied(x, y)) {
      len_ += eat ? 0 : 1;
      st_ = GS_OVER;
      sfx(SFX_GAMEOVER);
      if (score_ > best_) { best_ = score_; recordSet("snake", best_); toast("Новый рекорд!", 1500); }
      return;
    }
    head_ = (head_ + 1) % MAXLEN;
    bx_[head_] = x; by_[head_] = y;
    len_++;
    if (eat) {
      if (len_ > MAXLEN - 1) len_ = MAXLEN - 1;
      score_++;
      sfx(SFX_EAT);
      placeFood();
    }
  }
  void draw() override {
    char b[48];
    snprintf(b, sizeof(b), "Змейка  %lu  рек.%lu", (unsigned long)score_, (unsigned long)best_);
    drawHeader(b);
    gfx.drawFrame(0, OY - 1, SCR_W, ROWS * CELL + 2);
    for (int i = 0; i < len_; i++) {
      int k = (head_ - i + MAXLEN) % MAXLEN;
      int x = bx_[k] * CELL, y = OY + by_[k] * CELL;
      if (i == 0) gfx.drawBox(x, y, CELL, CELL);
      else gfx.drawBox(x, y, CELL - 1, CELL - 1);
    }
    if ((millis() / 200) % 4) gfx.drawDisc(fx_ * CELL + 1, OY + fy_ * CELL + 1, 1);
    if (st_ == GS_READY) drawGameOverlay("ЗМЕЙКА", "OK - старт", "крути / джойстик");
    if (st_ == GS_PAUSE) drawGameOverlay("Пауза", "OK - продолжить", "долго - выход");
    if (st_ == GS_OVER) {
      snprintf(b, sizeof(b), "Счёт: %lu", (unsigned long)score_);
      drawGameOverlay("Конец игры", b, "OK - заново");
    }
  }
};

Screen *scrSnake() { static Snake s; return &s; }

// =====================================================================
//  Ловец звёзд: ловите звёзды, уворачивайтесь от бомб
// =====================================================================
class Catcher : public Screen {
  struct Obj { float x, y, vy; bool bomb, alive; };
  static const int NOBJ = 6, PW = 18;
  Obj o_[NOBJ];
  float px_ = 64;
  int lives_ = 3;
  uint32_t score_ = 0, best_ = 0, spawnAt_ = 0, lastT_ = 0;
  GameState st_ = GS_READY;

  void reset() {
    for (auto &o : o_) o.alive = false;
    px_ = 64; lives_ = 3; score_ = 0;
  }
  void spawn() {
    for (auto &o : o_) if (!o.alive) {
      o.alive = true;
      o.x = random(4, 124);
      o.y = 11;
      o.vy = 18 + random(0, 14) + score_ * 0.8f;
      o.bomb = random(100) < 22 + min<uint32_t>(score_, 30);
      return;
    }
  }

 public:
  bool gamepad() override { return true; }
  bool keepAwake() override { return st_ == GS_PLAY; }
  void enter() override { best_ = recordGet("catch"); reset(); st_ = GS_READY; }
  void event(Ev e) override {
    if (e == EV_BACK) { uiPop(); return; }
    if (e == EV_OK) {
      if (st_ == GS_PLAY) st_ = GS_PAUSE;
      else { if (st_ != GS_PAUSE) reset(); st_ = GS_PLAY; lastT_ = millis(); sfx(SFX_START); }
      return;
    }
    if (st_ == GS_PLAY && e == EV_NEXT) px_ += 9;
    if (st_ == GS_PLAY && e == EV_PREV) px_ -= 9;
  }
  void update() override {
    uint32_t now = millis();
    float dt = (now - lastT_) / 1000.0f;
    lastT_ = now;
    if (st_ != GS_PLAY) return;
    if (dt > 0.1f) dt = 0.1f;
    px_ += joy.x * 1.4f * dt;
    px_ = constrain(px_, PW / 2.0f, SCR_W - PW / 2.0f);
    if (now >= spawnAt_) { spawn(); spawnAt_ = now + max<int>(250, 900 - (int)score_ * 15); }
    for (auto &o : o_) {
      if (!o.alive) continue;
      o.y += o.vy * dt;
      if (o.y >= 57 && o.y <= 62 && fabsf(o.x - px_) < PW / 2 + 2) {
        o.alive = false;
        if (o.bomb) {
          lives_--;
          sfx(SFX_ERROR);
          if (lives_ <= 0) {
            st_ = GS_OVER;
            sfx(SFX_GAMEOVER);
            if (score_ > best_) { best_ = score_; recordSet("catch", best_); toast("Новый рекорд!", 1500); }
          }
        } else { score_++; sfx(SFX_COIN); }
      } else if (o.y > 64) o.alive = false;
    }
  }
  void draw() override {
    char b[48];
    snprintf(b, sizeof(b), "Звёзды %lu  рек.%lu", (unsigned long)score_, (unsigned long)best_);
    drawHeader(b);
    for (int i = 0; i < lives_; i++) gfx.drawDisc(4 + i * 6, 15, 2);
    for (auto &o : o_) {
      if (!o.alive) continue;
      int x = (int)o.x, y = (int)o.y;
      if (o.bomb) { gfx.drawDisc(x, y, 3); gfx.drawLine(x, y - 3, x + 2, y - 6); }
      else { gfx.drawLine(x - 3, y, x + 3, y); gfx.drawLine(x, y - 3, x, y + 3); gfx.drawLine(x - 2, y - 2, x + 2, y + 2); gfx.drawLine(x - 2, y + 2, x + 2, y - 2); }
    }
    int x0 = (int)(px_ - PW / 2);
    gfx.drawRBox(x0, 59, PW, 4, 1);
    gfx.drawVLine(x0, 56, 3);
    gfx.drawVLine(x0 + PW - 1, 56, 3);
    if (st_ == GS_READY) drawGameOverlay("ЛОВЕЦ ЗВЁЗД", "ловите звёзды", "бомбы - мимо!");
    if (st_ == GS_PAUSE) drawGameOverlay("Пауза", "OK - продолжить", nullptr);
    if (st_ == GS_OVER) { snprintf(b, sizeof(b), "Звёзд: %lu", (unsigned long)score_); drawGameOverlay("Конец игры", b, "OK - заново"); }
  }
};

Screen *scrCatcher() { static Catcher s; return &s; }

// =====================================================================
//  Тест реакции
// =====================================================================
class Reaction : public Screen {
  enum { R_IDLE, R_WAIT, R_GO, R_RESULT, R_EARLY } st_ = R_IDLE;
  uint32_t goAt_ = 0, result_ = 0, best_ = 0;
  uint32_t last_[5] = {0};
  uint8_t n_ = 0;

 public:
  bool keepAwake() override { return st_ == R_WAIT || st_ == R_GO; }
  bool gamepad() override { return true; }
  void enter() override { st_ = R_IDLE; best_ = recordGet("react"); }
  void event(Ev e) override {
    if (e == EV_BACK) { uiPop(); return; }
    if (e != EV_OK) return;
    uint32_t now = millis();
    switch (st_) {
      case R_IDLE: case R_RESULT: case R_EARLY:
        st_ = R_WAIT;
        goAt_ = now + random(1500, 5000);
        break;
      case R_WAIT:
        st_ = R_EARLY;
        sfx(SFX_ERROR);
        break;
      case R_GO:
        result_ = now - goAt_;
        last_[n_ % 5] = result_; n_++;
        st_ = R_RESULT;
        if (!best_ || result_ < best_) { best_ = result_; recordSet("react", best_); sfx(SFX_WIN); }
        break;
    }
  }
  void update() override {
    if (st_ == R_WAIT && (int32_t)(millis() - goAt_) >= 0) { st_ = R_GO; beep(2500, 80, true); }
  }
  void draw() override {
    char b[48];
    if (st_ == R_GO) {
      gfx.drawBox(0, 0, SCR_W, SCR_H);
      gfx.setDrawColor(0);
      gfx.setFont(F_TITLE);
      drawCenteredUTF8(38, "ЖМИ!");
      gfx.setDrawColor(1);
      return;
    }
    drawHeader("Тест реакции");
    gfx.setFont(F_MAIN);
    switch (st_) {
      case R_IDLE:
        drawCenteredUTF8(30, "Нажмите OK, затем");
        drawCenteredUTF8(43, "жмите, когда экран");
        drawCenteredUTF8(56, "станет белым");
        break;
      case R_WAIT:
        drawCenteredUTF8(38, "Ждите...");
        break;
      case R_EARLY:
        gfx.setFont(F_TITLE);
        drawCenteredUTF8(36, "Рано!");
        gfx.setFont(F_SMALL);
        drawCenteredUTF8(52, "OK - ещё раз");
        break;
      case R_RESULT: {
        gfx.setFont(F_BIG);
        snprintf(b, sizeof(b), "%lu", (unsigned long)result_);
        int w = gfx.getStrWidth(b);
        gfx.drawStr((SCR_W - w - 16) / 2, 40, b);
        gfx.setFont(F_MAIN);
        gfx.drawUTF8((SCR_W + w - 16) / 2 + 2, 40, "мс");
        uint32_t sum = 0; uint8_t c = min<uint8_t>(n_, 5);
        for (uint8_t i = 0; i < c; i++) sum += last_[i];
        snprintf(b, sizeof(b), "средн. %lu  рекорд %lu", (unsigned long)(sum / max<uint8_t>(c, 1)), (unsigned long)best_);
        gfx.setFont(F_TINY);
        drawCenteredUTF8(52, b);
        const char *rank = result_ < 200 ? "Молния!" : result_ < 260 ? "Отлично" : result_ < 350 ? "Неплохо" : "Сонно...";
        gfx.setFont(F_SMALL);
        drawCenteredUTF8(62, rank);
        break;
      }
      default: break;
    }
  }
};

Screen *scrReaction() { static Reaction s; return &s; }

// =====================================================================
//  Терменвокс: высота тона от расстояния до руки (сонар)
// =====================================================================
class Theremin : public Screen {
  bool on_ = true;
  bool quantize_ = true;
  float freq_ = 0;
  uint8_t wave_[64];
  uint8_t wi_ = 0;

  static const char *noteName(float f, int &octave) {
    static const char *N[12] = {"C", "C#", "D", "D#", "E", "F", "F#", "G", "G#", "A", "A#", "B"};
    int n = (int)lroundf(12.0f * log2f(f / 440.0f)) + 57;  // 57 = A4
    octave = n / 12;
    return N[((n % 12) + 12) % 12];
  }

 public:
  bool keepAwake() override { return true; }
  bool gamepad() override { return true; }  // джойстик Y — сдвиг октавы, а не меню
  void enter() override { memset(wave_, 0, sizeof(wave_)); freq_ = 0; }
  void leave() override { soundStop(); }
  void event(Ev e) override {
    if (e == EV_OK) { on_ = !on_; if (!on_) soundStop(); return; }
    if (e == EV_NEXT || e == EV_PREV) { quantize_ = !quantize_; toast(quantize_ ? "Режим: ноты" : "Режим: плавно", 900); return; }
    if (e == EV_BACK) uiPop();
  }
  void update() override {
    float target = 0;
    if (S.sonarOk && S.distCm >= 4 && S.distCm <= 60) {
      // 4 см -> 1760 Гц, 60 см -> 110 Гц (экспоненциально, 4 октавы)
      float k = (S.distCm - 4) / 56.0f;
      target = 1760.0f * powf(2.0f, -4.0f * k);
      if (joy.present && cfg.joyEnabled) target *= powf(2.0f, joy.y / -100.0f);  // джойстик — ±октава
      if (quantize_) target = 440.0f * powf(2.0f, roundf(12.0f * log2f(target / 440.0f)) / 12.0f);
    }
    freq_ = target ? (freq_ ? freq_ + (target - freq_) * 0.5f : target) : 0;
    if (on_) soundTone(freq_ ? (uint16_t)freq_ : 0);
    wave_[wi_] = freq_ ? (uint8_t)constrain(log2f(freq_ / 110.0f) * 5, 0, 20) : 0;
    wi_ = (wi_ + 1) % 64;
  }
  void draw() override {
    drawHeader(on_ ? "Терменвокс" : "Терменвокс (тихо)");
    for (int i = 0; i < 64; i++) {
      uint8_t v = wave_[(wi_ + i) % 64];
      if (v) gfx.drawVLine(i * 2, 63 - v, v);
    }
    if (!freq_) {
      gfx.setFont(F_SMALL);
      drawCenteredUTF8(26, "Поднесите руку");
      drawCenteredUTF8(36, "к сонару (4-60 см)");
      return;
    }
    int oct;
    const char *nn = noteName(freq_, oct);
    char b[16];
    snprintf(b, sizeof(b), "%s%d", nn, oct);
    gfx.setFont(F_TITLE);
    gfx.drawStr(4, 28, b);
    gfx.setFont(F_SMALL);
    snprintf(b, sizeof(b), "%.0f Гц", freq_);
    drawRightUTF8(SCR_W - 2, 22, b);
    snprintf(b, sizeof(b), "%.0f см", S.distCm);
    drawRightUTF8(SCR_W - 2, 32, b);
    gfx.setFont(F_TINY);
    gfx.drawUTF8(4, 38, quantize_ ? "ноты" : "плавно");
  }
};

Screen *scrTheremin() { static Theremin s; return &s; }
