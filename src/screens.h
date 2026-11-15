#pragma once
#include "ui.h"

// Доступ к экранам (каждый — статический синглтон в своём .cpp)
Screen *scrMainMenu();
Screen *scrDashboard();
Screen *scrGraph();
Screen *scrSonar();
Screen *scrJoystick();
Screen *scrSecurity();
Screen *scrPinEntry();
Screen *scrRelay();
Screen *scrLog();
Screen *scrGames();
Screen *scrSnake();
Screen *scrCatcher();
Screen *scrReaction();
Screen *scrTheremin();
Screen *scrTools();
Screen *scrI2CScan();
Screen *scrWifiScan();
Screen *scrStopwatch();
Screen *scrFlashlight();
Screen *scrWifiInfo();
Screen *scrSettings();
Screen *scrInfo();
Screen *scrClock();
Screen *scrAlarmRing();
Screen *scrAlarmEdit();
Screen *scrRangefinder();
Screen *scrPeople();
Screen *scrLeds();
Screen *scrPong();
Screen *scrDraw();
Screen *scrMusic();

// Экран ввода PIN: после верного ввода вызывается onOk
void pinEntryStart(void (*onOk)());
void pinEntrySetNew();
