#include "leds.h"
#include "config.h"
#include "settings.h"
#include "sensors.h"
#include "logic.h"
#include "sound.h"
#include "clock.h"
#include <Adafruit_NeoPixel.h>

static Adafruit_NeoPixel strip(8, PIN_LEDS, NEO_GRB + NEO_KHZ800);
static uint8_t curCount = 0;
static uint32_t flashColor = 0, flashUntil = 0;
static uint8_t heat[64];  // для эффекта «огонь»

const char *ledsModeName(uint8_t m) {
  static const char *n[LEDS_MODE_COUNT] = {"выкл", "статус", "радуга", "темпер.", "огонь", "ночник", "сонар", "музыка"};
  return m < LEDS_MODE_COUNT ? n[m] : "?";
}

void ledsApply() {
  uint8_t n = min<uint8_t>(cfg.ledsCount, 64);
  if (n != curCount) {
    strip.clear();
    strip.show();
    strip.updateLength(n ? n : 1);
    curCount = n;
  }
  strip.setBrightness(cfg.ledsBright);
}

void ledsBegin() {
  strip.begin();
  ledsApply();
  strip.clear();
  strip.show();
}

void ledsFlash(uint32_t rgb, uint16_t ms) {
  flashColor = rgb;
  flashUntil = millis() + ms;
}

static void fill(uint32_t c) {
  for (uint8_t i = 0; i < curCount; i++) strip.setPixelColor(i, c);
}

static uint32_t hsv(uint16_t h, uint8_t s, uint8_t v) { return strip.gamma32(strip.ColorHSV(h, s, v)); }

void ledsUpdate() {
  static uint32_t last = 0;
  uint32_t now = millis();
  if (now - last < 33) return;
  last = now;
  if (!curCount) return;

  if (cfg.ledsMode == LEDS_OFF) { strip.clear(); strip.show(); return; }

  SecState sec = secState();
  if (sec == SEC_ALARM) {
    // «полицейская» мигалка
    bool ph = (now / 120) % 2;
    for (uint8_t i = 0; i < curCount; i++)
      strip.setPixelColor(i, ((i < curCount / 2) ^ ph) ? strip.Color(255, 0, 0) : strip.Color(0, 0, 255));
    strip.show();
    return;
  }
  if (clockAlarmRinging()) {  // будильник — «рассвет»
    uint8_t v = 80 + (uint8_t)(sinf(now / 300.0f) * 60 + 60);
    fill(strip.Color(v, v / 2, v / 8));
    strip.show();
    return;
  }

  switch (cfg.ledsMode) {
    case LEDS_STATUS: {
      float br = (sinf(now / 600.0f) + 1) / 2;  // «дыхание»
      uint8_t v = 20 + br * 100;
      uint32_t c;
      if (sec == SEC_ARMING) c = ((now / 250) % 2) ? strip.Color(255, 120, 0) : 0;
      else if (sec == SEC_ARMED) c = strip.Color(0, 0, v);
      else if (relayIsOn()) c = strip.Color(v, v / 3, 0);
      else c = strip.Color(0, v, v / 6);
      fill(c);
      break;
    }
    case LEDS_RAINBOW:
      for (uint8_t i = 0; i < curCount; i++) strip.setPixelColor(i, hsv(now * 20 + i * 65536L / curCount, 255, 255));
      break;
    case LEDS_TEMP: {
      float t = tempMain();
      if (isnan(t)) { fill(strip.Color(20, 20, 20)); break; }
      // 15 °C — синий, 30 °C — красный
      float k = constrain((t - 15.0f) / 15.0f, 0.0f, 1.0f);
      uint16_t hue = (uint16_t)(43690 * (1.0f - k));  // 240° -> 0°
      fill(hsv(hue, 255, 200));
      break;
    }
    case LEDS_FIRE: {
      uint8_t n = min<uint8_t>(curCount, 64);
      for (uint8_t i = 0; i < n; i++) heat[i] = max(0, (int)heat[i] - (int)random(0, 30));
      if (random(100) < 60) heat[random(n)] = 160 + random(95);
      for (uint8_t i = 0; i < n; i++) {
        uint8_t h = heat[i];
        strip.setPixelColor(i, strip.Color(h, h * h / 600, h > 220 ? (h - 220) * 2 : 0));
      }
      break;
    }
    case LEDS_NIGHT: {
      bool on = S.lastMotionMs && now - S.lastMotionMs < cfg.motionLightSec * 1000UL;
      static float lvl = 0;
      lvl += ((on ? 1.0f : 0.0f) - lvl) * 0.08f;  // плавное включение/выключение
      uint8_t v = lvl * 255;
      fill(strip.Color(v, v * 6 / 10, v / 4));
      break;
    }
    case LEDS_SONAR: {
      strip.clear();
      if (S.sonarOk) {
        float k = constrain(1.0f - S.distCm / 150.0f, 0.0f, 1.0f);  // ближе — больше и краснее
        uint8_t lit = (uint8_t)ceilf(k * curCount);
        uint16_t hue = (uint16_t)(21845 * (1.0f - k));  // зелёный -> красный
        for (uint8_t i = 0; i < lit; i++) strip.setPixelColor(i, hsv(hue, 255, 255));
      }
      break;
    }
    case LEDS_MUSIC: {
      uint16_t f = soundCurrentFreq();
      static float lvl = 0;
      static uint16_t hue = 0;
      if (f) { lvl = 1; hue = (uint16_t)(log2f(f / 200.0f) * 12000); }
      else lvl *= 0.85f;
      fill(hsv(hue, 255, (uint8_t)(lvl * 255)));
      break;
    }
  }

  if (flashUntil && (int32_t)(flashUntil - now) > 0) fill(flashColor);
  strip.show();
}
