#include "sensors.h"
#include "config.h"
#include "settings.h"
#include <DHT.h>
#include <OneWire.h>
#include <DallasTemperature.h>

SensorData S;
History hist[H_COUNT];
const uint16_t GRAPH_STEPS_SEC[] = {1, 5, 30, 60, 300};
const uint8_t  GRAPH_STEPS_COUNT = sizeof(GRAPH_STEPS_SEC) / sizeof(GRAPH_STEPS_SEC[0]);

static DHT dht(PIN_DHT, DHT_TYPE);
static OneWire oneWire(PIN_DS18B20);
static DallasTemperature ds(&oneWire);

static uint32_t tDht = 0, tDs = 0, tDsReq = 0, tSonar = 0, tHist = 0, tAnalog = 0, tDsScan = 0;
static bool dsPending = false;

// ---------------- Сонар без блокировки (прерывание по ECHO) ----------------
static volatile uint32_t echoStart = 0, echoLen = 0;
static volatile bool echoDone = false;
static bool sonarWaiting = false;
static uint32_t lastEchoMs = 0;
static float sonarBuf[5];
static uint8_t sonarIdx = 0, sonarFill = 0;

static void IRAM_ATTR echoIsr() {
  if (digitalRead(PIN_SONAR_ECHO)) {
    echoStart = micros();
  } else if (echoStart) {
    echoLen = micros() - echoStart;
    echoStart = 0;
    echoDone = true;
  }
}

float speedOfSound() {
  float t = tempMain();
  if (isnan(t)) t = 20.0f;
  return 331.3f + 0.606f * t;
}

static float median5(const float *a, uint8_t n) {
  float b[5];
  for (uint8_t i = 0; i < n; i++) b[i] = a[i];
  for (uint8_t i = 1; i < n; i++)
    for (uint8_t j = i; j > 0 && b[j - 1] > b[j]; j--) { float t = b[j]; b[j] = b[j - 1]; b[j - 1] = t; }
  return b[n / 2];
}

static void sonarUpdate(uint32_t now) {
  if (sonarWaiting && echoDone) {
    sonarWaiting = false;
    echoDone = false;
    float cm = echoLen * speedOfSound() / 20000.0f;  // мкс * м/с / 2 / 10000
    if (cm >= 2 && cm <= SONAR_MAX_CM) {
      sonarBuf[sonarIdx] = cm;
      sonarIdx = (sonarIdx + 1) % 5;
      if (sonarFill < 5) sonarFill++;
      S.distCm = median5(sonarBuf, sonarFill);
      lastEchoMs = now;
    }
  }
  if (now - tSonar >= SONAR_PERIOD_MS) {
    tSonar = now;
    sonarWaiting = true;
    echoDone = false;
    echoStart = 0;
    digitalWrite(PIN_SONAR_TRIG, HIGH);
    delayMicroseconds(10);
    digitalWrite(PIN_SONAR_TRIG, LOW);
  }
  bool ok = lastEchoMs && (now - lastEchoMs < 1500);
  if (!ok) { S.distCm = NAN; sonarFill = 0; }
  S.sonarOk = ok;
}

// ---------------- DS18B20 ----------------
static void dsScan() {
  ds.begin();
  S.dsCount = min<uint8_t>(ds.getDeviceCount(), SensorData::DS_MAX);
  if (S.dsCount) {
    ds.setResolution(12);
    ds.setWaitForConversion(false);
  }
}

static void dsUpdate(uint32_t now) {
  if (!S.dsCount) {
    S.dsOk = false;
    if (now - tDsScan > 10000) { tDsScan = now; dsScan(); }  // горячее подключение
    return;
  }
  if (!dsPending && now - tDs >= DS_PERIOD_MS) {
    tDs = now;
    ds.requestTemperatures();
    tDsReq = now;
    dsPending = true;
  }
  if (dsPending && now - tDsReq >= 760) {
    dsPending = false;
    bool any = false;
    for (uint8_t i = 0; i < S.dsCount; i++) {
      float t = ds.getTempCByIndex(i);
      if (t == DEVICE_DISCONNECTED_C || t < -55 || t > 125 || t == 85.0f) {
        S.dsT[i] = NAN;
      } else {
        S.dsT[i] = t;
        any = true;
      }
    }
    S.dsOk = any;
    if (!any) { S.dsCount = 0; tDsScan = now; }
  }
}

// ---------------- DHT ----------------
static void dhtUpdate(uint32_t now) {
  if (now - tDht < DHT_PERIOD_MS && tDht) return;
  tDht = now;
  float h = dht.readHumidity();
  float t = dht.readTemperature();
  if (isnan(h) || isnan(t)) {
    S.dhtErrors++;
    if (S.dhtErrors > 2) { S.dhtOk = false; S.dhtT = S.dhtH = S.dewPoint = NAN; }
    return;
  }
  S.dhtErrors = 0;
  S.dhtOk = true;
  S.dhtT = t;
  S.dhtH = h;
  // Точка росы (формула Магнуса)
  const float a = 17.27f, b = 237.7f;
  float g = a * t / (b + t) + log(max(h, 1.0f) / 100.0f);
  S.dewPoint = b * g / (a - g);
}

// ---------------- PIR ----------------
static void pirUpdate(uint32_t now) {
  if (!cfg.pirEnabled) { S.motion = false; return; }
  bool m = digitalRead(PIN_PIR) == HIGH;
  if (m && !S.motion) { S.motionCount++; }
  if (m) S.lastMotionMs = now;
  S.motion = m;
}

float tempMain() {
  if (S.dsOk && !isnan(S.dsT[0])) return S.dsT[0];
  if (S.dhtOk) return S.dhtT;
  return NAN;
}

float tempForThermostat() {
  if (cfg.thermoSource == 1) return S.dhtOk ? S.dhtT : NAN;
  return (S.dsOk && !isnan(S.dsT[0])) ? S.dsT[0] : NAN;
}

void sensorsBegin() {
  pinMode(PIN_SONAR_TRIG, OUTPUT);
  digitalWrite(PIN_SONAR_TRIG, LOW);
  pinMode(PIN_SONAR_ECHO, INPUT_PULLDOWN);   // без сонара вход не «плавает»
  attachInterrupt(digitalPinToInterrupt(PIN_SONAR_ECHO), echoIsr, CHANGE);
  pinMode(PIN_PIR, INPUT_PULLDOWN);          // без PIR — нет ложных срабатываний
  analogSetPinAttenuation(PIN_ANALOG, ADC_11db);

  dht.begin();
  dsScan();
  tDsScan = millis();
}

void sensorsUpdate() {
  uint32_t now = millis();
  sonarUpdate(now);
  dsUpdate(now);
  dhtUpdate(now);
  pirUpdate(now);

  if (now - tAnalog >= 100) {
    tAnalog = now;
    if (cfg.analogEnabled) {
      S.analogRaw = analogRead(PIN_ANALOG);
      S.analogPct = (uint32_t)S.analogRaw * 100 / 4095;
    }
  }

  uint32_t step = (uint32_t)GRAPH_STEPS_SEC[min<uint8_t>(cfg.graphStepIdx, GRAPH_STEPS_COUNT - 1)] * 1000;
  if (now - tHist >= step) {
    tHist = now;
    hist[H_DS].push(S.dsOk ? S.dsT[0] : NAN);
    hist[H_DHT_T].push(S.dhtT);
    hist[H_DHT_H].push(S.dhtH);
    hist[H_DIST].push(S.distCm);
    hist[H_ANALOG].push(cfg.analogEnabled ? (float)S.analogPct : NAN);
  }
}
