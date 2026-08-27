#pragma once
#include <Arduino.h>
#include "config.h"

enum AlertPattern { ALERT_OFF, ALERT_PERIODIC, ALERT_CONTINUOUS };

static AlertPattern g_alertPattern = ALERT_OFF;
static uint32_t     g_alertIntervalMs = 500;
static uint32_t     g_alertLastToggleMs = 0;
static bool         g_alertToneOn = false;
static bool         g_muted = false;
static uint8_t      g_ledBrightness = 200;
static uint32_t     g_pingUntilMs = 0;
static int          g_pingFreq = BEEP_FREQ_HZ;

inline void alertWriteLed(uint8_t brightness) {
#if LED_IS_RGB
  rgbLedWrite(PIN_LED, brightness, brightness, brightness);
#else
  analogWrite(PIN_LED, brightness);
#endif
}

inline void alertInit() {
  pinMode(PIN_LED, OUTPUT);
  alertWriteLed(0);
#if !LED_IS_RGB
  digitalWrite(PIN_LED, LOW);
#endif
  noTone(PIN_BUZZER);
}

inline void alertSetPattern(AlertPattern p, uint32_t intervalMs = 500) {
  if (p != g_alertPattern) { g_alertLastToggleMs = millis(); g_alertToneOn = false; }
  g_alertPattern = p;
  g_alertIntervalMs = intervalMs;
}

inline void alertSetLedBrightness(uint8_t b) { g_ledBrightness = b; }
inline void alertToggleMute() { g_muted = !g_muted; }
inline bool alertIsMuted() { return g_muted; }

// One-shot "ping": short beep + LED flash, used by the radar sweep when
// it crosses a flagged device - like a submarine sonar contact chirp.
inline void alertFirePing(int freq, uint8_t brightness) {
  g_pingFreq = freq;
  g_pingUntilMs = millis() + BEEP_ON_MS;
  g_ledBrightness = brightness;
}

inline void alertTick() {
  uint32_t now = millis();
  bool wantOn = false;
  int freqToPlay = BEEP_FREQ_HZ;

  if (now < g_pingUntilMs) {
    wantOn = true;
    freqToPlay = g_pingFreq;
  } else if (g_alertPattern == ALERT_CONTINUOUS) {
    wantOn = true;
  } else if (g_alertPattern == ALERT_PERIODIC) {
    uint32_t phaseLen = g_alertToneOn ? BEEP_ON_MS : g_alertIntervalMs;
    if (now - g_alertLastToggleMs >= phaseLen) {
      g_alertToneOn = !g_alertToneOn;
      g_alertLastToggleMs = now;
    }
    wantOn = g_alertToneOn;
  }

  // The LED always flashes on the same rhythm as the "would-be" beep, even
  // if muted, so you still get a visual alert with the sound off.
  if (wantOn) alertWriteLed(g_ledBrightness);
  else {
    alertWriteLed(0);
#if !LED_IS_RGB
    digitalWrite(PIN_LED, LOW);
#endif
  }

  if (wantOn && !g_muted) tone(PIN_BUZZER, freqToPlay);
  else noTone(PIN_BUZZER);
}
