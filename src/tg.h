#pragma once
#include <Arduino.h>

// Telegram-бот: уведомления и управление.
// Сеть работает в отдельной задаче FreeRTOS, интерфейс не подвисает.
void        tgBegin();
void        tgUpdate();                       // в loop(): выполняет пришедшие команды
void        tgNotify(const char *fmt, ...);   // поставить сообщение в очередь (неблокирующе)
const char *tgStatus();                        // состояние для экрана/веба
