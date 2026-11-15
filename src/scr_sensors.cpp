// Экраны датчиков: панель, графики, сонар, джойстик
#include "screens.h"
#include "display.h"
#include "sensors.h"
#include "settings.h"
#include "sound.h"
#include "logic.h"
#include "input.h"

// =====================================================================
//  Панель (дашборд): обзор + крупные страницы по каждому датчику
// =====================================================================
class Dashboard : public Screen {
  int  page_ = 0;
  bool autoRotate_ = false;
  uint32_t rotAt_ = 0;
  static const int PAGES = 6;

  // Плитка 63x26: рамка и подпись, значение рисует вызывающий
  void tile(int x, int y, const char *label) {
    gfx.drawRFrame(x, y, 63, 26, 3);
    gfx.setFont(F_TINY);
    gfx.drawUTF8(x + 4, y + 7, label);
  }
  void tileText(int x, int y, const char *value, bool bold = true) {
    gfx.setFont(bold ? F_BOLD : F_MAIN);
    gfx.drawUTF8(x + 4, y + 21, value);
  }

  void overview() {
    char b[20], v[16];
    // DS18B20
    tile(0, 11, "DS18B20");
    if (S.dsOk) drawTempValue(4, 32, S.dsT[0], F_BOLD, false);
    else tileText(0, 11, "--", false);
    // DHT
    tile(65, 11, "DHT11");
    if (S.dhtOk) {
      int w = drawTempValue(69, 32, S.dhtT, F_BOLD, false, 0);
      snprintf(b, sizeof(b), "%.0f%%", S.dhtH);
      gfx.drawStr(69 + w + 3, 32, b);
    } else tileText(65, 11, "--", false);
    // Сонар
    tile(0, 38, "СОНАР");
    if (S.sonarOk) { snprintf(b, sizeof(b), "%.0f см", S.distCm); tileText(0, 38, b); }
    else tileText(0, 38, "--", false);
    // Движение
    tile(65, 38, "ДВИЖЕНИЕ");
    if (!cfg.pirEnabled) snprintf(b, sizeof(b), "выкл");
    else if (S.motion) snprintf(b, sizeof(b), "ЕСТЬ!");
    else if (S.lastMotionMs) { fmtDuration(v, sizeof(v), (millis() - S.lastMotionMs) / 1000); snprintf(b, sizeof(b), "%s", v); }
    else snprintf(b, sizeof(b), "нет");
    tileText(65, 38, b, cfg.pirEnabled);
    if (S.motion && (millis() / 300) % 2) gfx.drawRFrame(66, 39, 61, 24, 2);
  }

  void bigTemp(const char *title, float c, const char *sub) {
    drawHeader(title);
    if (isnan(c)) {
      gfx.setFont(F_MAIN);
      drawCenteredUTF8(36, "Датчик не найден");
      gfx.setFont(F_TINY);
      drawCenteredUTF8(50, "проверьте подключение");
      return;
    }
    drawArt(Icons::THERMO, 16, 2, 18, 1);
    drawTempValue(24, 46, c, F_HUGE, true);
    gfx.setFont(F_SMALL);
    if (sub) drawCenteredUTF8(62, sub);
  }

  void humidityPage() {
    drawHeader("Влажность DHT11");
    if (!S.dhtOk) { gfx.setFont(F_MAIN); drawCenteredUTF8(38, "Датчик не найден"); return; }
    drawArt(Icons::DROP, 16, 2, 16, 1);
    char b[8];
    snprintf(b, sizeof(b), "%.0f", S.dhtH);
    gfx.setFont(F_HUGE);
    gfx.drawStr(26, 46, b);
    int w = gfx.getStrWidth(b);
    gfx.setFont(F_BOLD);
    gfx.drawStr(28 + w, 46, "%");
    drawProgress(4, 50, 120, 6, S.dhtH / 100.0f);
    // комфорт
    const char *mood = S.dhtH < 30 ? "сухо" : S.dhtH > 60 ? "влажно" : "комфорт";
    gfx.setFont(F_TINY);
    char t[48];
    snprintf(t, sizeof(t), "%s, точка росы %.1f%s", mood, cToUnit(S.dewPoint), unitStr());
    drawCenteredUTF8(63, t);
  }

  void sonarPage() {
    drawHeader("Расстояние");
    if (!S.sonarOk) { gfx.setFont(F_MAIN); drawCenteredUTF8(38, "Нет эха"); gfx.setFont(F_TINY); drawCenteredUTF8(50, "далеко или не подключён"); return; }
    drawArt(Icons::SONAR, 16, 2, 18, 1);
    char b[10];
    snprintf(b, sizeof(b), S.distCm < 100 ? "%.1f" : "%.0f", S.distCm);
    gfx.setFont(F_HUGE);
    gfx.drawStr(24, 46, b);
    gfx.setFont(F_BOLD);
    gfx.drawUTF8(26 + gfx.getStrWidth(b) + 0, 46, "см");
    drawProgress(4, 52, 120, 6, S.distCm / 200.0f);
  }

  void motionPage() {
    drawHeader("Движение");
    drawArt(Icons::PERSON, 16, 8, 22, 2);
    gfx.setFont(F_BOLD);
    gfx.drawUTF8(50, 28, !cfg.pirEnabled ? "PIR выкл" : S.motion ? "ЕСТЬ!" : "Тихо");
    gfx.setFont(F_SMALL);
    char b[40], d[16];
    snprintf(b, sizeof(b), "Срабат.: %lu", (unsigned long)S.motionCount);
    gfx.drawUTF8(50, 42, b);
    if (S.lastMotionMs) {
      fmtDuration(d, sizeof(d), (millis() - S.lastMotionMs) / 1000);
      snprintf(b, sizeof(b), "%s назад", d);
    } else snprintf(b, sizeof(b), "ещё не было");
    gfx.drawUTF8(50, 53, b);
    if (S.motion) {
      int r = (millis() / 60) % 20;
      gfx.drawCircle(24, 38, 20 + r / 2, U8G2_DRAW_UPPER_LEFT | U8G2_DRAW_LOWER_LEFT);
    }
  }

 public:
  void enter() override { rotAt_ = millis(); }
  void event(Ev e) override {
    switch (e) {
      case EV_NEXT: case EV_RIGHT: page_ = (page_ + 1) % PAGES; rotAt_ = millis(); break;
      case EV_PREV: page_ = (page_ + PAGES - 1) % PAGES; rotAt_ = millis(); break;
      case EV_OK:
        autoRotate_ = !autoRotate_;
        toast(autoRotate_ ? "Автолистание ВКЛ" : "Автолистание ВЫКЛ", 1200);
        rotAt_ = millis();
        break;
      default: Screen::event(e);
    }
  }
  bool keepAwake() override { return autoRotate_; }
  void update() override {
    if (autoRotate_ && millis() - rotAt_ > 4000) { page_ = (page_ + 1) % PAGES; rotAt_ = millis(); }
  }
  void draw() override {
    switch (page_) {
      case 0: drawHeader("Панель"); overview(); break;
      case 1: {
        char sub[32] = "";
        if (S.dsCount > 1) snprintf(sub, sizeof(sub), "датчиков: %d, #2: %.1f%s", S.dsCount, cToUnit(S.dsT[1]), unitStr());
        bigTemp("DS18B20", S.dsOk ? S.dsT[0] : NAN, sub);
        break;
      }
      case 2: {
        char sub[32] = "";
        if (S.dhtOk) snprintf(sub, sizeof(sub), "влажность %.0f%%", S.dhtH);
        bigTemp("DHT11", S.dhtOk ? S.dhtT : NAN, sub);
        break;
      }
      case 3: humidityPage(); break;
      case 4: sonarPage(); break;
      case 5: motionPage(); break;
    }
    // индикатор страниц
    for (int i = 0; i < PAGES; i++) {
      int x = SCR_W - 4 - (PAGES - 1 - i) * 4;
      if (page_ == 0) break;
      if (i == page_) gfx.drawBox(x - 1, 12, 3, 3); else gfx.drawPixel(x, 13);
    }
  }
};

Screen *scrDashboard() { static Dashboard s; return &s; }

// =====================================================================
//  Графики
// =====================================================================
class GraphScreen : public Screen {
  int series_ = 0;
  static const char *name(int i) {
    static const char *n[H_COUNT] = {"DS18B20", "DHT11 темп.", "Влажность", "Расстояние", "Аналог"};
    return n[i];
  }
  static bool isTemp(int i) { return i == H_DS || i == H_DHT_T; }

 public:
  void event(Ev e) override {
    switch (e) {
      case EV_NEXT: case EV_RIGHT: series_ = (series_ + 1) % H_COUNT; break;
      case EV_PREV: series_ = (series_ + H_COUNT - 1) % H_COUNT; break;
      case EV_OK: hist[series_].clear(); toast("История очищена", 1000); break;
      default: Screen::event(e);
    }
  }
  void draw() override {
    drawHeader(name(series_));
    const History &h = hist[series_];
    float mn = 1e9, mx = -1e9, last = NAN;
    int valid = 0;
    for (int i = 0; i < h.count; i++) {
      float v = h.at(i);
      if (isnan(v)) continue;
      if (isTemp(series_)) v = cToUnit(v);
      mn = min(mn, v); mx = max(mx, v); last = v; valid++;
    }
    if (valid < 2) {
      gfx.setFont(F_MAIN);
      drawCenteredUTF8(36, "Мало данных");
      gfx.setFont(F_TINY);
      drawCenteredUTF8(50, "подождите или смените шаг");
      return;
    }
    if (mx - mn < 1.0f) { float c = (mx + mn) / 2; mn = c - 0.5f; mx = c + 0.5f; }

    // область графика
    const int gx = 24, gy = 13, gw = SCR_W - gx - 1, gh = 40;
    gfx.setFont(F_TINY);
    char b[12];
    fmtFloat(b, sizeof(b), mx, mx - mn < 10 ? 1 : 0); gfx.drawStr(0, gy + 5, b);
    fmtFloat(b, sizeof(b), mn, mx - mn < 10 ? 1 : 0); gfx.drawStr(0, gy + gh, b);
    for (int x = gx; x < gx + gw; x += 3) gfx.drawPixel(x, gy + gh / 2);  // пунктир середины
    gfx.drawVLine(gx - 2, gy, gh + 1);
    gfx.drawHLine(gx - 2, gy + gh, gw + 2);

    int prevX = -1, prevY = -1;
    for (int i = 0; i < h.count; i++) {
      float v = h.at(i);
      int x = gx + (int)((long)(HIST_LEN - h.count + i) * (gw - 1) / (HIST_LEN - 1));  // новые точки справа
      if (isnan(v)) { prevX = -1; continue; }
      if (isTemp(series_)) v = cToUnit(v);
      int y = gy + gh - (int)((v - mn) / (mx - mn) * gh);
      if (prevX >= 0) gfx.drawLine(prevX, prevY, x, y);
      else gfx.drawPixel(x, y);
      prevX = x; prevY = y;
    }
    // текущее значение
    gfx.setFont(F_SMALL);
    char cur[24];
    const char *unit = isTemp(series_) ? unitStr() : series_ == H_DIST ? "см" : "%";
    snprintf(cur, sizeof(cur), "%.1f%s", last, unit);
    gfx.drawUTF8(gx, 63, cur);
    uint16_t step = GRAPH_STEPS_SEC[min<uint8_t>(cfg.graphStepIdx, GRAPH_STEPS_COUNT - 1)];
    char span[16];
    fmtDuration(span, sizeof(span), (uint32_t)h.count * step);
    drawRightUTF8(SCR_W, 63, span);
  }
};

Screen *scrGraph() { static GraphScreen s; return &s; }

// =====================================================================
//  Сонар: «радар» + парктроник со звуком
// =====================================================================
class SonarScreen : public Screen {
  bool parking_ = false;
  uint32_t nextBeep_ = 0;
  float sweep_ = 0;
  int8_t sweepDir_ = 1;
  uint8_t blips_[32];  // расстояние по углу (0 = нет)

 public:
  void enter() override { memset(blips_, 0, sizeof(blips_)); }
  void leave() override { if (parking_) soundStop(); parking_ = false; }
  bool keepAwake() override { return parking_; }
  void event(Ev e) override {
    if (e == EV_OK) {
      parking_ = !parking_;
      toast(parking_ ? "Парктроник ВКЛ" : "Парктроник ВЫКЛ", 1000);
      return;
    }
    Screen::event(e);
  }
  void update() override {
    sweep_ += sweepDir_ * 0.9f;
    if (sweep_ >= 31) { sweep_ = 31; sweepDir_ = -1; }
    if (sweep_ <= 0) { sweep_ = 0; sweepDir_ = 1; }
    blips_[(int)sweep_] = S.sonarOk ? (uint8_t)min(200.0f, S.distCm) : 0;

    if (parking_ && S.sonarOk && S.distCm < 150) {
      uint32_t now = millis();
      uint32_t period = S.distCm < 10 ? 0 : (uint32_t)(S.distCm * 6);
      if (period == 0) { soundTone(2200); }
      else if ((int32_t)(now - nextBeep_) >= 0) {
        soundStop();
        beep(2200, 40, true);
        nextBeep_ = now + period;
      }
    } else if (parking_) {
      soundStop();
    }
  }
  void draw() override {
    drawHeader(parking_ ? "Сонар · парктроник" : "Сонар");
    // радар-полукруг: центр внизу слева
    const int cx = 40, cy = 63, R = 50;
    gfx.drawCircle(cx, cy, R, U8G2_DRAW_UPPER_LEFT | U8G2_DRAW_UPPER_RIGHT);
    gfx.drawCircle(cx, cy, R * 2 / 3, U8G2_DRAW_UPPER_LEFT | U8G2_DRAW_UPPER_RIGHT);
    gfx.drawCircle(cx, cy, R / 3, U8G2_DRAW_UPPER_LEFT | U8G2_DRAW_UPPER_RIGHT);
    float a = PI - sweep_ / 31.0f * PI;
    gfx.drawLine(cx, cy, cx + (int)(cosf(a) * R), cy - (int)(sinf(a) * R));
    for (int i = 0; i < 32; i++) {
      if (!blips_[i]) continue;
      float ang = PI - i / 31.0f * PI;
      float r = blips_[i] / 200.0f * R;
      int bx = cx + (int)(cosf(ang) * r), by = cy - (int)(sinf(ang) * r);
      gfx.drawBox(bx - 1, by - 1, 2, 2);
    }
    // цифры справа
    gfx.setFont(F_BIG);
    char b[8];
    if (S.sonarOk) snprintf(b, sizeof(b), "%.0f", S.distCm); else snprintf(b, sizeof(b), "--");
    drawRightUTF8(SCR_W - 2, 40, b);
    gfx.setFont(F_SMALL);
    drawRightUTF8(SCR_W - 2, 50, "см");
    if (S.sonarOk) {
      int h = (int)(min(S.distCm, 200.0f) / 200.0f * 48);
      gfx.drawFrame(92, 13, 4, 50);
      gfx.drawBox(93, 62 - h, 2, h);
    }
    gfx.setFont(F_TINY);
    drawRightUTF8(SCR_W, 63, "OK: звук");
  }
};

Screen *scrSonar() { static SonarScreen s; return &s; }

// =====================================================================
//  Джойстик: визуализация и калибровка
// =====================================================================
class JoyScreen : public Screen {
  int trail_[16][2];
  uint8_t ti_ = 0;

 public:
  bool gamepad() override { return true; }  // джойстик не листает меню здесь
  void enter() override { memset(trail_, 0, sizeof(trail_)); }
  void event(Ev e) override {
    if (e == EV_OK) {
      joyCalibrate();
      toast("Центр откалиброван", 1200);
      return;
    }
    if (e == EV_BACK) uiPop();
  }
  void draw() override {
    drawHeader("Джойстик");
    if (!joy.present || !cfg.joyEnabled) {
      gfx.setFont(F_MAIN);
      drawCenteredUTF8(34, "Не обнаружен");
      gfx.setFont(F_TINY);
      drawCenteredUTF8(46, "VRx→34 VRy→35 SW→13");
      drawCenteredUTF8(56, "подключите и перезагрузите");
      return;
    }
    const int cx = 32, cy = 37, r = 24;
    gfx.drawCircle(cx, cy, r);
    gfx.drawHLine(cx - r, cy, 2 * r + 1);
    gfx.drawVLine(cx, cy - r, 2 * r + 1);
    int dx = cx + joy.x * (r - 3) / 100, dy = cy + joy.y * (r - 3) / 100;
    trail_[ti_][0] = dx; trail_[ti_][1] = dy; ti_ = (ti_ + 1) % 16;
    for (auto &t : trail_) if (t[0]) gfx.drawPixel(t[0], t[1]);
    if (joy.btn) gfx.drawDisc(dx, dy, 5); else gfx.drawCircle(dx, dy, 4);

    gfx.setFont(F_SMALL);
    char b[20];
    snprintf(b, sizeof(b), "X %4d", joy.x); gfx.drawStr(66, 22, b);
    snprintf(b, sizeof(b), "Y %4d", joy.y); gfx.drawStr(66, 31, b);
    snprintf(b, sizeof(b), "raw %d", joy.rawX); gfx.drawStr(66, 41, b);
    snprintf(b, sizeof(b), "raw %d", joy.rawY); gfx.drawStr(66, 49, b);
    gfx.drawUTF8(66, 58, joy.btn ? "Кнопка: ДА" : "Кнопка: нет");
    gfx.setFont(F_TINY);
    drawRightUTF8(SCR_W, 64, "OK - калибровка");
  }
};

Screen *scrJoystick() { static JoyScreen s; return &s; }
