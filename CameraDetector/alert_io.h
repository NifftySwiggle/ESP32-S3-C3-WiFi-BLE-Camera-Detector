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

enum VolumeLevel {
  VOL_MUTE = 0,
  VOL_LOW  = 1,
  VOL_MED  = 2,
  VOL_HIGH = 3,
  VOL_COUNT = 4
};

static VolumeLevel g_volume = VOL_HIGH;

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

inline void alertSetVolume(VolumeLevel v) {
  g_volume = (VolumeLevel)constrain((int)v, 0, VOL_COUNT - 1);
  g_muted = (g_volume == VOL_MUTE);
}

inline VolumeLevel alertGetVolume() {
  return g_volume;
}

inline void alertCycleVolume() {
  g_volume = (VolumeLevel)(((int)g_volume + 1) % VOL_COUNT);
  g_muted = (g_volume == VOL_MUTE);
}

inline const char* alertVolumeLabel(VolumeLevel v) {
  switch (v) {
    case VOL_MUTE: return "MUTE (0%)";
    case VOL_LOW:  return "LOW (30%)";
    case VOL_MED:  return "MED (70%)";
    case VOL_HIGH: return "HIGH (100%)";
    default:       return "HIGH (100%)";
  }
}

inline void alertToggleMute() {
  if (g_volume == VOL_MUTE) {
    g_volume = VOL_HIGH;
    g_muted = false;
  } else {
    g_volume = VOL_MUTE;
    g_muted = true;
  }
}

inline bool alertIsMuted() { return (g_volume == VOL_MUTE || g_muted); }

// One-shot "ping": short beep + LED flash, used by the radar sweep when
// it crosses a flagged device - like a submarine sonar contact chirp.
inline void alertFirePing(int freq, uint8_t brightness) {
  g_pingFreq = freq;
  g_pingUntilMs = millis() + BEEP_ON_MS;
  g_ledBrightness = brightness;
}

struct SfxNote {
  uint16_t freq;
  uint16_t durationMs;
};
#define MAX_SFX_NOTES 6
static SfxNote  g_sfxQueue[MAX_SFX_NOTES];
static uint8_t  g_sfxCount = 0;
static uint8_t  g_sfxIdx = 0;
static uint32_t g_sfxNoteEndMs = 0;

inline void alertPlaySfx(const SfxNote *notes, uint8_t count) {
  if (count > MAX_SFX_NOTES) count = MAX_SFX_NOTES;
  memcpy(g_sfxQueue, notes, count * sizeof(SfxNote));
  g_sfxCount = count;
  g_sfxIdx = 0;
  g_sfxNoteEndMs = millis() + g_sfxQueue[0].durationMs;
}

inline void alertSfxSteer() {
  SfxNote n[] = { {1800, 20} };
  alertPlaySfx(n, 1);
}

inline void alertSfxCoin() {
  SfxNote n[] = { {2200, 40}, {3200, 60} };
  alertPlaySfx(n, 2);
}

inline void alertSfxCrash() {
  SfxNote n[] = { {750, 60}, {400, 80}, {220, 140} };
  alertPlaySfx(n, 3);
  alertWriteLed(255);
}

inline void alertSfxHighScore() {
  SfxNote n[] = { {1400, 60}, {1800, 60}, {2200, 60}, {2800, 150} };
  alertPlaySfx(n, 4);
}

inline void alertSfxGeigerTick() {
  SfxNote n[] = { {3500, 8} };
  alertPlaySfx(n, 1);
}

inline void alertSfxNewDevice() {
  SfxNote n[] = { {2000, 30}, {3000, 45} };
  alertPlaySfx(n, 2);
  alertWriteLed(255);
}

inline void alertTick() {
  uint32_t now = millis();
  bool wantOn = false;
  int freqToPlay = BEEP_FREQ_HZ;

  if (g_sfxCount > 0) {
    if (now >= g_sfxNoteEndMs) {
      g_sfxIdx++;
      if (g_sfxIdx >= g_sfxCount) {
        g_sfxCount = 0;
      } else {
        g_sfxNoteEndMs = now + g_sfxQueue[g_sfxIdx].durationMs;
      }
    }
    if (g_sfxCount > 0) {
      freqToPlay = g_sfxQueue[g_sfxIdx].freq;
      wantOn = (freqToPlay > 0);
    }
  } else if (now < g_pingUntilMs) {
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

  if (wantOn && g_volume != VOL_MUTE && !g_muted) {
    if (g_volume == VOL_HIGH) {
      tone(PIN_BUZZER, freqToPlay);
    } else if (g_volume == VOL_MED) {
      if ((now % 4) < 3) tone(PIN_BUZZER, freqToPlay);
      else noTone(PIN_BUZZER);
    } else if (g_volume == VOL_LOW) {
      if ((now % 4) == 0) tone(PIN_BUZZER, freqToPlay);
      else noTone(PIN_BUZZER);
    }
  } else {
    noTone(PIN_BUZZER);
  }
}

