#include "tg.h"
#include "config.h"
#include "settings.h"
#include "sensors.h"
#include "logic.h"
#include "sound.h"
#include "display.h"
#include "net.h"
#include "clock.h"
#include <stdarg.h>
#include <WiFi.h>
#include <WiFiClientSecure.h>
#include <HTTPClient.h>
#define ARDUINOJSON_USE_LONG_LONG 1
#include <ArduinoJson.h>

enum TgCmd : uint8_t { TG_ARM = 1, TG_DISARM, TG_RELAY_ON, TG_RELAY_OFF, TG_BEEP, TG_BIND };

struct CmdMsg {
  uint8_t cmd;
  char    chat[24];
};

static const size_t MSG_LEN = 256;
static QueueHandle_t outQ = nullptr;   // исходящие сообщения (char[MSG_LEN])
static QueueHandle_t cmdQ = nullptr;   // команды для основного цикла
static volatile uint8_t state = 0;     // 0 выкл, 1 нет сети, 2 ок, 3 ошибка
static char lastError[24] = "";

const char *tgStatus() {
  if (!cfg.tgOn) return "выкл";
  if (!cfg.tgToken[0]) return "нет токена";
  switch (state) {
    case 1: return "нет интернета";
    case 2: return cfg.tgChat[0] ? "работает" : "напишите боту";
    case 3: return lastError[0] ? lastError : "ошибка";
    default: return "запуск";
  }
}

void tgNotify(const char *fmt, ...) {
  if (!outQ || !cfg.tgOn || !cfg.tgToken[0] || !cfg.tgChat[0]) return;
  char buf[MSG_LEN];
  va_list ap;
  va_start(ap, fmt);
  vsnprintf(buf, sizeof(buf), fmt, ap);
  va_end(ap);
  xQueueSend(outQ, buf, 0);
}

static void buildStatus(char *b, size_t n) {
  char t[16], d[16], h[8], ds[16];
  fmtFloat(t, sizeof(t), S.dsOk ? S.dsT[0] : NAN, 1);
  fmtFloat(d, sizeof(d), S.dhtT, 1);
  fmtFloat(h, sizeof(h), S.dhtH, 0);
  fmtFloat(ds, sizeof(ds), S.distCm, 0);
  char clk[8];
  clockFmtHM(clk, sizeof(clk));
  snprintf(b, n,
           "📟 VeryGood ESP32 (%s)\n"
           "🌡 DS18B20: %s °C\n"
           "🌡 DHT11: %s °C, 💧 %s %%\n"
           "📏 Сонар: %s см\n"
           "🚶 Движение: %s (всего %lu)\n"
           "🛡 Охрана: %s, тревог %lu\n"
           "🔌 Реле: %s (%s)",
           clk, t, d, h, ds, S.motion ? "ЕСТЬ" : "нет", (unsigned long)S.motionCount,
           secStateName(), (unsigned long)secAlarmCount(), relayIsOn() ? "ВКЛ" : "ВЫКЛ",
           relayModeName(cfg.relayMode));
}

// ---------------- HTTP (только из задачи tgTask) ----------------
static WiFiClientSecure *client = nullptr;
static HTTPClient http;  // один объект — соединение TLS переиспользуется

static bool apiCall(const char *method, const String &body, String &resp) {
  http.setTimeout(8000);
  http.setReuse(true);
  String url = String("https://api.telegram.org/bot") + cfg.tgToken + "/" + method;
  if (!http.begin(*client, url)) { strlcpy(lastError, "ошибка URL", sizeof(lastError)); return false; }
  http.addHeader("Content-Type", "application/json");
  int code = http.POST(body);
  resp = code > 0 ? http.getString() : String();
  http.end();
  if (code == 200) return true;
  if (code == 401 || code == 404) strlcpy(lastError, "неверный токен", sizeof(lastError));
  else snprintf(lastError, sizeof(lastError), "HTTP %d", code);
  return false;
}

static bool sendTo(const char *chat, const char *text) {
  JsonDocument d;
  d["chat_id"] = chat;
  d["text"] = text;
  String body, resp;
  serializeJson(d, body);
  return apiCall("sendMessage", body, resp);
}

static void pushCmd(uint8_t c, const char *chat) {
  CmdMsg m;
  m.cmd = c;
  strlcpy(m.chat, chat, sizeof(m.chat));
  xQueueSend(cmdQ, &m, 0);
}

static void handleText(const char *chat, const char *text) {
  // Привязка: первый написавший становится владельцем
  if (!cfg.tgChat[0]) {
    pushCmd(TG_BIND, chat);
    char b[160];
    snprintf(b, sizeof(b), "✅ Бот привязан к этому чату (id %s).\nКоманды: /status /arm /disarm /relay_on /relay_off /beep", chat);
    sendTo(chat, b);
    return;
  }
  if (strcmp(chat, cfg.tgChat) != 0) {
    sendTo(chat, "⛔ Нет доступа");
    return;
  }
  // отрезаем @имя_бота
  char cmd[24];
  strlcpy(cmd, text, sizeof(cmd));
  char *at = strchr(cmd, '@'); if (at) *at = 0;
  char *sp = strchr(cmd, ' '); if (sp) *sp = 0;

  char b[MSG_LEN];
  if (!strcmp(cmd, "/status") || !strcmp(cmd, "/start")) { buildStatus(b, sizeof(b)); sendTo(chat, b); }
  else if (!strcmp(cmd, "/arm"))       { pushCmd(TG_ARM, chat); }
  else if (!strcmp(cmd, "/disarm"))    { pushCmd(TG_DISARM, chat); }
  else if (!strcmp(cmd, "/relay_on"))  { pushCmd(TG_RELAY_ON, chat); }
  else if (!strcmp(cmd, "/relay_off")) { pushCmd(TG_RELAY_OFF, chat); }
  else if (!strcmp(cmd, "/beep"))      { pushCmd(TG_BEEP, chat); }
  else sendTo(chat, "Команды:\n/status — показания\n/arm — на охрану\n/disarm — снять\n/relay_on, /relay_off — реле\n/beep — пискнуть");
}

static void tgTask(void *) {
  client = new WiFiClientSecure();
  client->setInsecure();  // без проверки сертификата — проще и экономит память
  int64_t offset = 0;
  bool firstPoll = true;
  char msg[MSG_LEN];
  uint32_t lastPoll = 0;

  for (;;) {
    if (!cfg.tgOn || !cfg.tgToken[0]) { state = 0; firstPoll = true; vTaskDelay(pdMS_TO_TICKS(2000)); continue; }
    if (!netStaConnected()) { state = 1; vTaskDelay(pdMS_TO_TICKS(2000)); continue; }

    // Исходящие: ждём до 500 мс
    if (xQueueReceive(outQ, msg, pdMS_TO_TICKS(500)) == pdTRUE) {
      if (cfg.tgChat[0]) state = sendTo(cfg.tgChat, msg) ? 2 : 3;
      continue;
    }

    if (millis() - lastPoll < 3000) continue;
    lastPoll = millis();

    JsonDocument req;
    req["offset"] = offset;
    req["timeout"] = 0;
    req["limit"] = 5;
    req["allowed_updates"][0] = "message";
    String body, resp;
    serializeJson(req, body);
    if (!apiCall("getUpdates", body, resp)) { state = 3; continue; }
    state = 2;

    JsonDocument doc;
    if (deserializeJson(doc, resp)) continue;
    for (JsonObject u : doc["result"].as<JsonArray>()) {
      offset = u["update_id"].as<long long>() + 1;
      if (firstPoll) continue;  // старые сообщения после перезагрузки не выполняем
      const char *text = u["message"]["text"] | "";
      char chat[24];
      snprintf(chat, sizeof(chat), "%lld", u["message"]["chat"]["id"].as<long long>());
      if (*text) handleText(chat, text);
    }
    if (firstPoll) {
      firstPoll = false;
      if (cfg.tgChat[0]) sendTo(cfg.tgChat, "🔄 VeryGood ESP32 на связи. /status — показания");
    }
  }
}

void tgBegin() {
  outQ = xQueueCreate(6, MSG_LEN);
  cmdQ = xQueueCreate(6, sizeof(CmdMsg));
  xTaskCreatePinnedToCore(tgTask, "tg", 12288, nullptr, 1, nullptr, 0);
}

void tgUpdate() {
  if (!cmdQ) return;
  CmdMsg m;
  while (xQueueReceive(cmdQ, &m, 0) == pdTRUE) {
    switch (m.cmd) {
      case TG_BIND:
        strlcpy(cfg.tgChat, m.chat, sizeof(cfg.tgChat));
        settingsSave();
        toast("Telegram привязан", 2000);
        logEvent("Telegram: чат %s", m.chat);
        break;
      case TG_ARM:
        secArm();
        tgNotify("🛡 %s", secStateName());
        break;
      case TG_DISARM:
        secDisarm();
        tgNotify("🔓 Охрана снята");
        break;
      case TG_RELAY_ON:
      case TG_RELAY_OFF:
        relaySetManual(m.cmd == TG_RELAY_ON);
        settingsSave();
        tgNotify("🔌 Реле: %s", m.cmd == TG_RELAY_ON ? "ВКЛ" : "ВЫКЛ");
        break;
      case TG_BEEP:
        beep(2000, 300, true);
        toast("Привет из Telegram!", 2000);
        tgNotify("🔔 Пискнул");
        break;
    }
  }
}
