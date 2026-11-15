#pragma once
#include <Arduino.h>
#include "input.h"

// Базовый класс экрана. Экраны складываются в стек: uiPush / uiPop.
class Screen {
 public:
  virtual ~Screen() {}
  virtual void enter() {}
  virtual void leave() {}
  virtual void event(Ev e);              // по умолчанию BACK/LEFT = назад
  virtual void update() {}               // вызывается каждый кадр перед draw()
  virtual void draw() = 0;
  virtual bool keepAwake() { return false; }    // запрет заставки
  virtual bool gamepad() { return false; }      // джойстик работает как геймпад
};

void    uiBegin(Screen *root);
void    uiLoop();
void    uiPush(Screen *s);
void    uiPop();
void    uiHome();
Screen *uiTop();

// ---------- Универсальное меню-список ----------
struct MenuItem {
  const char *label;
  void (*action)();
};

class ListMenu : public Screen {
 public:
  ListMenu(const char *title, const MenuItem *items, uint8_t n) : title_(title), items_(items), n_(n) {}
  void event(Ev e) override;
  void draw() override;

 protected:
  const char     *title_;
  const MenuItem *items_;
  uint8_t         n_;
  int             sel_ = 0, top_ = 0;
};

// Отрисовка строки списка с плавно скользящим курсором
static const int LIST_ROWS = 4;
static const int LIST_ROW_H = 13;
void listMoveSel(int &sel, int &top, int n, int delta, bool wrap = true);
void listDrawRow(int row, const char *label, const char *value, bool selected, bool editing);
void listDrawCursor(int row);   // анимированный курсор
void listSnapCursor(int row);

// ---------- Мини-редактор параметров (строки с функциями) ----------
struct EdRow {
  const char *label;
  void (*fmt)(char *b, size_t n);   // текст значения
  void (*change)(int d);            // d = +1 / -1 (для toggle: 0 = переключить)
  bool toggle;                      // OK меняет сразу, без режима правки
};

class RowEditor : public Screen {
 public:
  RowEditor(const char *title, const EdRow *rows, uint8_t n) : title_(title), rows_(rows), n_(n) {}
  void enter() override { edit_ = false; }
  void leave() override;
  void event(Ev e) override;
  void draw() override;

 protected:
  const char  *title_;
  const EdRow *rows_;
  uint8_t      n_;
  int          sel_ = 0, top_ = 0;
  bool         edit_ = false;
};
