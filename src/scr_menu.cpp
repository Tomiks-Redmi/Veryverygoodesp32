// Главное меню-карусель и подменю
#include "screens.h"
#include "display.h"
#include "sound.h"
#include "clock.h"

struct CarouselItem {
  const char        *label;
  const char *const *icon;
  Screen *(*target)();
};

static const CarouselItem MAIN_ITEMS[] = {
  {"Панель",      Icons::DASH,   scrDashboard},
  {"Часы",        Icons::CLOCK,  scrClock},
  {"Графики",     Icons::GRAPH,  scrGraph},
  {"Сонар",       Icons::SONAR,  scrSonar},
  {"Охрана",      Icons::SHIELD, scrSecurity},
  {"Реле",        Icons::THERMO, scrRelay},
  {"Джойстик",    Icons::JOY,    scrJoystick},
  {"Игры",        Icons::GAME,   scrGames},
  {"Инструменты", Icons::TOOLS,  scrTools},
  {"WiFi",        Icons::WIFI,   scrWifiInfo},
  {"Настройки",   Icons::GEAR,   scrSettings},
  {"О системе",   Icons::INFO,   scrInfo},
};
static const int MAIN_N = sizeof(MAIN_ITEMS) / sizeof(MAIN_ITEMS[0]);

class MainMenu : public Screen {
  int   target_ = 0;      // может выходить за 0..N-1 (бесконечная карусель)
  float pos_ = 0;

  static int wrap(int i) { return (i % MAIN_N + MAIN_N) % MAIN_N; }

 public:
  void event(Ev e) override {
    switch (e) {
      case EV_NEXT: case EV_RIGHT: target_++; break;
      case EV_PREV: case EV_LEFT:  target_--; break;
      case EV_OK:
        sfx(SFX_OK);
        uiPush(MAIN_ITEMS[wrap(target_)].target());
        break;
      default: break;  // корневой экран: «назад» некуда
    }
  }

  void update() override {
    pos_ += (target_ - pos_) * 0.35f;
    if (fabsf(target_ - pos_) < 0.02f) pos_ = target_;
    // держим числа маленькими
    if (pos_ == target_ && (target_ > 1000 || target_ < -1000)) { target_ = wrap(target_); pos_ = target_; }
  }

  void draw() override {
    char t[24], hm[8];
    if (clockValid()) { clockFmtHM(hm, sizeof(hm)); snprintf(t, sizeof(t), "%s  %d/%d", hm, wrap(target_) + 1, MAIN_N); }
    else snprintf(t, sizeof(t), "Меню %d/%d", wrap(target_) + 1, MAIN_N);
    drawHeader(t);

    const int spacing = 46, iconY = 14;
    int base = (int)floorf(pos_);
    for (int k = base - 2; k <= base + 3; k++) {
      float off = (k - pos_) * spacing;
      int x = 48 + (int)lroundf(off);
      if (x <= -32 || x >= SCR_W) continue;
      drawArt(MAIN_ITEMS[wrap(k)].icon, 16, x, iconY, 2);
    }
    // рамка выбора
    gfx.drawRFrame(44, iconY - 3, 40, 38, 5);

    // точки-индикатор
    int dotsW = MAIN_N * 4;
    int dx = (SCR_W - dotsW) / 2;
    for (int i = 0; i < MAIN_N; i++) {
      if (i == wrap(target_)) gfx.drawBox(dx + i * 4, 50, 3, 2);
      else gfx.drawPixel(dx + i * 4 + 1, 51);
    }

    gfx.setFont(F_MAIN);
    drawCenteredUTF8(63, MAIN_ITEMS[wrap(target_)].label);
  }
};

Screen *scrMainMenu() { static MainMenu s; return &s; }

// ---------------- Подменю ----------------
static const MenuItem GAMES_ITEMS[] = {
  {"Змейка",          [] { uiPush(scrSnake()); }},
  {"Ловец звёзд",     [] { uiPush(scrCatcher()); }},
  {"Тест реакции",    [] { uiPush(scrReaction()); }},
  {"Терменвокс",      [] { uiPush(scrTheremin()); }},
  {"Понг",            [] { uiPush(scrPong()); }},
  {"Рисовалка",       [] { uiPush(scrDraw()); }},
  {"Музыка",          [] { uiPush(scrMusic()); }},
};

static const MenuItem TOOLS_ITEMS[] = {
  {"Рулетка",         [] { uiPush(scrRangefinder()); }},
  {"Счётчик проходов",[] { uiPush(scrPeople()); }},
  {"RGB-подсветка",   [] { uiPush(scrLeds()); }},
  {"I2C сканер",      [] { uiPush(scrI2CScan()); }},
  {"WiFi сканер",     [] { uiPush(scrWifiScan()); }},
  {"Секундомер",      [] { uiPush(scrStopwatch()); }},
  {"Журнал событий",  [] { uiPush(scrLog()); }},
  {"Фонарик",         [] { uiPush(scrFlashlight()); }},
};

Screen *scrGames() { static ListMenu s("Игры", GAMES_ITEMS, sizeof(GAMES_ITEMS) / sizeof(GAMES_ITEMS[0])); return &s; }
Screen *scrTools() { static ListMenu s("Инструменты", TOOLS_ITEMS, sizeof(TOOLS_ITEMS) / sizeof(TOOLS_ITEMS[0])); return &s; }
