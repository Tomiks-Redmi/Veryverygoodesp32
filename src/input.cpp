#include "input.h"
#include "config.h"
#include "settings.h"

JoyState joy;

// ---------------- Очередь событий ----------------
static const uint8_t QSIZE = 16;
static Ev queue[QSIZE];
static uint8_t qHead = 0, qTail = 0;
static uint32_t lastActivity = 0;
static bool joyNav = true;

void inputPush(Ev e) {
  uint8_t next = (qHead + 1) % QSIZE;
  if (next == qTail) return;  // переполнено — теряем
  queue[qHead] = e;
  qHead = next;
  lastActivity = millis();
}

Ev inputPoll() {
  if (qHead == qTail) return EV_NONE;
  Ev e = queue[qTail];
  qTail = (qTail + 1) % QSIZE;
  return e;
}

void inputClear() { qHead = qTail = 0; }
uint32_t inputLastActivity() { return lastActivity; }
void inputTouch() { lastActivity = millis(); }
void inputSetJoyNav(bool on) { joyNav = on; }

// ---------------- Энкодер (прерывания, таблица переходов) ----------------
static volatile int32_t encRaw = 0;
static volatile uint8_t encState = 0;
static int32_t encConsumed = 0;
static const int8_t ENC_TABLE[16] = {0, -1, 1, 0, 1, 0, 0, -1, -1, 0, 0, 1, 0, 1, -1, 0};

static void IRAM_ATTR encIsr() {
  uint8_t s = (digitalRead(PIN_ENC_A) << 1) | digitalRead(PIN_ENC_B);
  encState = ((encState << 2) | s) & 0x0F;
  encRaw += ENC_TABLE[encState];
}

// ---------------- Кнопки с антидребезгом ----------------
struct Button {
  uint8_t  pin;
  bool     stable = false;     // true = нажата
  bool     lastRead = false;
  uint32_t changedAt = 0;
  uint32_t pressedAt = 0;
  bool     longFired = false;

  void begin(uint8_t p) { pin = p; pinMode(pin, INPUT_PULLUP); }

  // Возвращает: 0 — ничего, 1 — короткий клик, 2 — долгое нажатие
  uint8_t update(uint32_t now) {
    bool r = digitalRead(pin) == LOW;
    if (r != lastRead) { lastRead = r; changedAt = now; }
    if (now - changedAt < 25 || r == stable) {
      if (stable && !longFired && now - pressedAt >= LONG_PRESS_MS) {
        longFired = true;
        return 2;
      }
      return 0;
    }
    stable = r;
    if (stable) { pressedAt = now; longFired = false; return 0; }
    // отпущена
    return longFired ? 0 : 1;
  }
};

static Button encBtn, joyBtn;

// ---------------- Джойстик ----------------
static int joyCX = 2048, joyCY = 2048;
static int8_t joyDir = 0;        // текущее удерживаемое направление (Ev) или 0
static uint32_t joyNextRepeat = 0;

static int readAvg(uint8_t pin) {
  int s = 0;
  for (int i = 0; i < 4; i++) s += analogRead(pin);
  return s / 4;
}

void joyCalibrate() {
  long sx = 0, sy = 0;
  for (int i = 0; i < 32; i++) { sx += analogRead(PIN_JOY_X); sy += analogRead(PIN_JOY_Y); delay(1); }
  joyCX = sx / 32;
  joyCY = sy / 32;
}

// Плавающий (неподключённый) вход АЦП сильно шумит или лежит на краю шкалы.
static bool detectJoystick() {
  int mnx = 4095, mxx = 0, mny = 4095, mxy = 0;
  long sx = 0, sy = 0;
  for (int i = 0; i < 48; i++) {
    int x = analogRead(PIN_JOY_X), y = analogRead(PIN_JOY_Y);
    mnx = min(mnx, x); mxx = max(mxx, x); mny = min(mny, y); mxy = max(mxy, y);
    sx += x; sy += y;
    delayMicroseconds(700);
  }
  int ax = sx / 48, ay = sy / 48;
  bool stable = (mxx - mnx) < 250 && (mxy - mny) < 250;
  bool centered = ax > 1000 && ax < 3100 && ay > 1000 && ay < 3100;
  return stable && centered;
}

static int scaleAxis(int raw, int center) {
  int d = raw - center;
  int span = d >= 0 ? (4095 - center) : center;
  if (span < 100) span = 100;
  int v = (long)d * 100 / span;
  if (abs(v) < 12) v = 0;  // мёртвая зона
  return constrain(v, -100, 100);
}

void inputBegin() {
  pinMode(PIN_ENC_A, INPUT_PULLUP);
  pinMode(PIN_ENC_B, INPUT_PULLUP);
  encState = (digitalRead(PIN_ENC_A) << 1) | digitalRead(PIN_ENC_B);
  attachInterrupt(digitalPinToInterrupt(PIN_ENC_A), encIsr, CHANGE);
  attachInterrupt(digitalPinToInterrupt(PIN_ENC_B), encIsr, CHANGE);
  encBtn.begin(PIN_ENC_BTN);
  joyBtn.begin(PIN_JOY_BTN);

  analogReadResolution(12);
  analogSetPinAttenuation(PIN_JOY_X, ADC_11db);
  analogSetPinAttenuation(PIN_JOY_Y, ADC_11db);
  joy.present = detectJoystick();
  if (joy.present) joyCalibrate();
  lastActivity = millis();
}

bool encButtonDown() { return encBtn.stable; }

void inputUpdate() {
  uint32_t now = millis();

  // --- энкодер ---
  int32_t raw = encRaw;  // 32-битное чтение атомарно на ESP32
  int32_t steps = (raw - encConsumed) / ENC_STEPS_PER_DETENT;
  if (steps != 0) {
    encConsumed += steps * ENC_STEPS_PER_DETENT;
    bool cw = steps > 0;
    if (cfg.encInvert) cw = !cw;
    int n = abs(steps);
    if (n > 4) n = 4;
    while (n--) inputPush(cw ? EV_NEXT : EV_PREV);
  }

  // --- кнопки ---
  uint8_t r = encBtn.update(now);
  if (r == 1) inputPush(EV_OK);
  else if (r == 2) inputPush(EV_BACK);

  bool joyUsable = joy.present && cfg.joyEnabled;
  r = joyBtn.update(now);
  joy.btn = joyBtn.stable && joyUsable;
  if (joyUsable) {
    if (r == 1) inputPush(EV_OK);
    else if (r == 2) inputPush(EV_BACK);
  }

  // --- джойстик ---
  if (!joyUsable) { joy.x = joy.y = 0; return; }
  joy.rawX = readAvg(PIN_JOY_X);
  joy.rawY = readAvg(PIN_JOY_Y);
  joy.x = scaleAxis(joy.rawX, joyCX);
  joy.y = scaleAxis(joy.rawY, joyCY);
  if (cfg.joyInvertX) joy.x = -joy.x;
  if (cfg.joyInvertY) joy.y = -joy.y;

  if (!joyNav) { joyDir = 0; return; }

  int8_t dir = 0;
  if (abs(joy.x) > 60 || abs(joy.y) > 60) {
    if (abs(joy.x) > abs(joy.y)) dir = joy.x > 0 ? EV_RIGHT : EV_LEFT;
    else                         dir = joy.y > 0 ? EV_NEXT : EV_PREV;
  } else if (abs(joy.x) < 40 && abs(joy.y) < 40) {
    dir = 0;
  } else {
    dir = joyDir;  // гистерезис
  }

  if (dir != joyDir) {
    joyDir = dir;
    if (dir) { inputPush((Ev)dir); joyNextRepeat = now + 400; }
  } else if (dir && (int32_t)(now - joyNextRepeat) >= 0) {
    inputPush((Ev)dir);
    joyNextRepeat = now + 110;
  }
}
