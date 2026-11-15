#include "ui.h"
#include "config.h"
#include "display.h"
#include "settings.h"
#include "sensors.h"
#include "logic.h"
#include "sound.h"
#include "screens.h"
#include "clock.h"

static const uint8_t STACK_MAX = 8;
static Screen *stack[STACK_MAX];
static uint8_t depth = 0;
static uint32_t lastFrame = 0;
static bool saver = false;

void Screen::event(Ev e) {
  if (e == EV_BACK || e == EV_LEFT) uiPop();
}

Screen *uiTop() { return depth ? stack[depth - 1] : nullptr; }

static void applyJoyMode() {
  Screen *t = uiTop();
  inputSetJoyNav(!(t && t->gamepad()));
}

void uiPush(Screen *s) {
  if (!s || depth >= STACK_MAX) return;
  stack[depth++] = s;
  applyJoyMode();
  s->enter();
}

void uiPop() {
  if (depth <= 1) return;  // корневой экран не закрываем
  Screen *s = stack[--depth];
  s->leave();
  sfx(SFX_BACK);
  applyJoyMode();
  inputClear();
}

void uiHome() {
  while (depth > 1) { stack[--depth]->leave(); }
  applyJoyMode();
}

void uiBegin(Screen *root) {
  depth = 0;
  uiPush(root);
  lastFrame = millis();
}

// ---------------- Заставка (защита OLED от выгорания) ----------------
static void drawSaver() {
  static int16_t x = 20, y = 20;
  static int8_t dx = 1, dy = 1;
  static uint32_t moveAt = 0;
  static uint8_t stars[12][3];
  static bool init = false;
  uint32_t now = millis();
  if (!init) {
    for (auto &s : stars) { s[0] = random(128); s[1] = random(64); s[2] = random(1, 4); }
    init = true;
  }
  if (now - moveAt > 120) {
    moveAt = now;
    x += dx; y += dy;
    if (x < 0 || x > 128 - 70) { dx = -dx; x += dx; }
    if (y < 22 || y > 63) { dy = -dy; y += dy; }
    for (auto &s : stars) {
      if (s[0] < s[2]) { s[0] = 127; s[1] = random(64); } else s[0] -= s[2];
    }
  }
  for (auto &s : stars) gfx.drawPixel(s[0], s[1]);
  float t = tempMain();
  char hm[8];
  if (clockValid()) {
    clockFmtHM(hm, sizeof(hm));
    gfx.setFont(F_BIG);
    gfx.drawStr(x, y, hm);
    if (!isnan(t)) {
      gfx.setFont(F_SMALL);
      int yy = y + 9 > 63 ? y - 25 : y + 9;
      drawTempValue(x, yy, t, F_SMALL, false);
    }
    return;
  }
  if (!isnan(t)) {
    drawTempValue(x, y, t, F_BIG, true);
  } else {
    gfx.setFont(F_MAIN);
    gfx.drawUTF8(x, y, "VeryGood");
  }
  if (S.dhtOk) {
    char b[12];
    snprintf(b, sizeof(b), "%.0f%%", S.dhtH);
    gfx.setFont(F_SMALL);
    gfx.drawStr(x, y + 9 > 63 ? y - 25 : y + 9, b);
  }
}

void uiLoop() {
  uint32_t now = millis();
  Screen *top = uiTop();
  if (!top) return;

  // Тревога — сразу показываем экран охраны
  if (secState() == SEC_ALARM && top != scrSecurity() && top != scrPinEntry()) {
    saver = false;
    uiPush(scrSecurity());
    top = uiTop();
  }

  // Будильник
  if (clockAlarmRinging() && secState() != SEC_ALARM && top != scrAlarmRing()) {
    saver = false;
    uiPush(scrAlarmRing());
    top = uiTop();
  }

  // Пробуждение от движения
  if (saver && cfg.wakeOnMotion && S.motion) { saver = false; inputTouch(); }

  Ev e;
  while ((e = inputPoll()) != EV_NONE) {
    if (saver) { saver = false; inputClear(); break; }
    if ((e == EV_NEXT || e == EV_PREV) && !top->gamepad()) sfx(SFX_CLICK);
    top->event(e);
    top = uiTop();
  }

  if (!saver && cfg.screensaverMin && !top->keepAwake() &&
      now - inputLastActivity() > cfg.screensaverMin * 60000UL) {
    saver = true;
  }

  if (now - lastFrame < UI_FRAME_MS) return;
  lastFrame = now;

  gfx.clearBuffer();
  if (saver) {
    drawSaver();
  } else {
    top->update();
    top = uiTop();
    top->draw();
    drawToast();
  }
  gfx.sendBuffer();
}

// ---------------- Списки ----------------
static float cursorY = -1;

void listMoveSel(int &sel, int &top, int n, int delta, bool wrap) {
  if (n <= 0) return;
  sel += delta;
  if (wrap) sel = (sel % n + n) % n;
  else sel = constrain(sel, 0, n - 1);
  if (sel < top) top = sel;
  if (sel >= top + LIST_ROWS) top = sel - LIST_ROWS + 1;
}

void listSnapCursor(int row) { cursorY = HEADER_H + 1 + row * LIST_ROW_H; }

void listDrawCursor(int row) {
  float target = HEADER_H + 1 + row * LIST_ROW_H;
  if (cursorY < 0 || fabsf(cursorY - target) > 4 * LIST_ROW_H) cursorY = target;
  cursorY += (target - cursorY) * 0.45f;
  if (fabsf(cursorY - target) < 0.5f) cursorY = target;
  gfx.setDrawColor(1);
  gfx.drawRBox(0, (int)cursorY, SCR_W - 5, LIST_ROW_H, 3);
}

void listDrawRow(int row, const char *label, const char *value, bool selected, bool editing) {
  int y = HEADER_H + 1 + row * LIST_ROW_H;
  int base = y + 10;
  gfx.setFont(F_MAIN);
  gfx.setFontMode(1);
  gfx.setDrawColor(2);  // XOR — текст инвертируется на курсоре
  gfx.drawUTF8(4, base, label);
  if (value) {
    int w = gfx.getUTF8Width(value);
    int vx = SCR_W - 8 - w;
    if (editing && selected) {
      // редактируемое значение — светлый текст в тёмной «таблетке»
      gfx.setDrawColor(0);
      gfx.drawRBox(vx - 3, y + 1, w + 6, LIST_ROW_H - 2, 2);
      gfx.setDrawColor(2);
    }
    gfx.drawUTF8(vx, base, value);
  }
  gfx.setDrawColor(1);
  gfx.setFontMode(0);
}

void ListMenu::event(Ev e) {
  switch (e) {
    case EV_NEXT: listMoveSel(sel_, top_, n_, 1); break;
    case EV_PREV: listMoveSel(sel_, top_, n_, -1); break;
    case EV_OK:
    case EV_RIGHT:
      if (items_[sel_].action) { sfx(SFX_OK); items_[sel_].action(); }
      break;
    default: Screen::event(e);
  }
}

void ListMenu::draw() {
  char t[32];
  snprintf(t, sizeof(t), "%s  %d/%d", title_, sel_ + 1, n_);
  drawHeader(t);
  listDrawCursor(sel_ - top_);
  for (int i = 0; i < LIST_ROWS && top_ + i < n_; i++) {
    listDrawRow(i, items_[top_ + i].label, nullptr, top_ + i == sel_, false);
  }
  drawScrollbar(sel_, n_);
}

// ---------------- RowEditor ----------------
void RowEditor::leave() { settingsSave(); }

void RowEditor::event(Ev e) {
  const EdRow &r = rows_[sel_];
  if (edit_) {
    switch (e) {
      case EV_NEXT: case EV_RIGHT: r.change(+1); break;
      case EV_PREV: case EV_LEFT:  r.change(-1); break;
      case EV_OK: case EV_BACK:    edit_ = false; sfx(SFX_OK); break;
      default: break;
    }
    return;
  }
  switch (e) {
    case EV_NEXT: listMoveSel(sel_, top_, n_, 1); break;
    case EV_PREV: listMoveSel(sel_, top_, n_, -1); break;
    case EV_OK: case EV_RIGHT:
      sfx(SFX_OK);
      if (r.toggle) r.change(0);
      else edit_ = true;
      break;
    default: Screen::event(e);
  }
}

void RowEditor::draw() {
  drawHeader(title_);
  listDrawCursor(sel_ - top_);
  char v[24];
  for (int i = 0; i < LIST_ROWS && top_ + i < n_; i++) {
    const EdRow &r = rows_[top_ + i];
    v[0] = 0;
    if (r.fmt) r.fmt(v, sizeof(v));
    listDrawRow(i, r.label, v[0] ? v : nullptr, top_ + i == sel_, edit_);
  }
  drawScrollbar(sel_, n_);
}
