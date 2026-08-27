// ============================================================
//  WiFi / BLE Camera & Spy-Device Detector  (v3)
//  ESP32-C3 or ESP32-S3; the hardware profile is selected by the board target
//
//  RADAR   - sweeping radar; beeps + LED flash each time the sweep
//            crosses a flagged, untrusted device within range
//  FINDER  - beeps/LED flash faster as signal gets stronger
//
//  Button 1 short press      -> switch RADAR <-> FINDER
//  Button 1 long press       -> open MENU (from Radar/Finder)
//    in MENU:  turn pot=move highlight, Button 1 short=select, long=back
//    Menu -> "Known Devices": turn pot=browse detected devices,
//            BOOT short=toggle trust on/off for that one device
//    in CALIBRATE: Button 1 short=advance step, long=cancel
//  2nd button (if wired):
//    Radar: short/long = mute
//    Finder: short = cycle target, long = TRUST current target
// ============================================================

#include <math.h>
#include <Preferences.h>
#include <esp_system.h>
#include "config.h"
#include "oui_database.h"
#include "trusted_store.h"
#include "candidate_store.h"
#include "wifi_scanner.h"
#include "ble_scanner.h"
#include "alert_io.h"
#include "buttons.h"
#include "display_ui.h"

enum AppState { ST_RADAR, ST_FINDER, ST_MENU, ST_CALIBRATE, ST_DEVICE_LIST, ST_GAME };
static AppState g_state = ST_RADAR;
static AppState g_prevOperatingState = ST_RADAR;   // Radar/Finder to return to

enum MenuItem { MI_MODE, MI_GAME, MI_CALIBRATE, MI_DEVICES, MI_TRUST_ALL, MI_CLEAR_TRUST, MI_MUTE, MI_RESET_CALIB, MI_EXIT, MI_COUNT };
static int g_menuIndex = 0;
static int g_deviceListIdx = 0;
static int g_gameRound = 0;
static int g_gameScore = 0;
static int g_gameLane = 1;
static int g_gameObstacleLane[3] = {0, 1, 2};
static int g_gameObstacleY[3] = {-1, -1, -1};
static int g_gameLaneBag[3] = {0, 1, 2};
static int g_gameLaneBagPos = 3;
static bool g_gameStarted = false;
static uint32_t g_gameObstacleAccumMs = 0;
static bool g_gameFinished = false;
static uint32_t g_gameNextObstacleMs = 0;
static uint32_t g_gameLastTickMs = 0;

enum CalibStep { CAL_SELECT1, CAL_CONFIRM1, CAL_SAMPLE1, CAL_RESULT1, CAL_SETDIST2, CAL_SAMPLE2, CAL_RESULT2 };
static CalibStep g_calStep = CAL_SELECT1;
static int       g_calSelectIdx = 0;
static uint8_t   g_calTargetMac[6];
static float     g_calRssiSum = 0;
static int       g_calRssiCount = 0;
static uint32_t  g_calSampleStartMs = 0;
static int8_t    g_calRssi1m = 0;
static float     g_calDist2 = 5.0f;
static int8_t    g_calRssiD2 = 0;
static uint32_t  g_calNoSignalUntil = 0;

static int g_btnMode = -1;
static int g_btnSelect = -1;
static int g_finderTargetIdx = -1;   // -1 = auto-pick strongest untrusted
static uint32_t g_trustedFlashUntil = 0;

static const float RANGE_PRESETS[] = {2, 5, 10, 15, 20, 30};
static const int NUM_RANGE_PRESETS = sizeof(RANGE_PRESETS) / sizeof(RANGE_PRESETS[0]);
static int g_rangePresetIdx = 2;      // used only if USE_RANGE_POT == 0

// Runtime calibration (loaded from / saved to flash via Preferences)
static int   g_txRef1m   = TX_REF_RSSI_1M;
static float g_pathLossN = PATH_LOSS_N;

static float    g_sweepAngle = 0;
static uint32_t g_lastSweepUpdateMs = 0;

Preferences g_prefs;

void loadCalibration() {
  g_prefs.begin("camdet", true);
  g_txRef1m   = g_prefs.getInt("txref", TX_REF_RSSI_1M);
  g_pathLossN = g_prefs.getFloat("pathn", PATH_LOSS_N);
  g_prefs.end();
}

void persistCalibration() {
  g_prefs.begin("camdet", false);
  g_prefs.putInt("txref", g_txRef1m);
  g_prefs.putFloat("pathn", g_pathLossN);
  g_prefs.end();
}

void resetCalibrationDefaults() {
  g_txRef1m = TX_REF_RSSI_1M;
  g_pathLossN = PATH_LOSS_N;
  persistCalibration();
}

void setup() {
  Serial.begin(115200);
  delay(200);
  Serial.println("\nCamera Detector booting...");
  randomSeed(esp_random() ^ analogRead(PIN_RANGE_POT) ^ micros());

  Serial.println("[boot] loading settings");
  loadCalibration();
  trustedLoad();
  Serial.println("[boot] initializing display");
  displayInit();
  Serial.println("[boot] initializing alerts");
  alertInit();
  Serial.println("[boot] initializing storage");
  candidateStoreInit();

  g_btnMode = buttonRegister(PIN_BTN_MODE);
#if USE_SELECT_BTN
  g_btnSelect = buttonRegister(PIN_BTN_SELECT);
#endif
#if USE_RANGE_POT
  pinMode(PIN_RANGE_POT, INPUT);
  analogReadResolution(12);
#endif

  displaySplash();
  Serial.println("[boot] initializing WiFi scanner");
  wifiScannerInit();
  Serial.println("[boot] initializing BLE scanner");
  bleScannerInit();
  g_lastSweepUpdateMs = millis();

  Serial.printf("Ready. ref=%ddBm N=%.2f trusted=%d\n", g_txRef1m, g_pathLossN, g_trusted.count);
}

float potToMeters() {
  int raw = analogRead(PIN_RANGE_POT);
  return RANGE_MIN_M + (raw / 4095.0f) * (RANGE_MAX_M - RANGE_MIN_M);
}

int potToIndex(int count) {
  int raw = analogRead(PIN_RANGE_POT);
  int idx = (raw * count) / 4096;
  if (idx >= count) idx = count - 1;
  if (idx < 0) idx = 0;
  return idx;
}

float currentRangeMeters() {
#if USE_RANGE_POT
  static float smoothed = -1;
  float target = potToMeters();
  smoothed = (smoothed < 0) ? target : (smoothed * 0.85f + target * 0.15f);
  return smoothed;
#else
  return RANGE_PRESETS[g_rangePresetIdx];
#endif
}

int8_t rssiThresholdForRange(float meters) {
  if (meters < 0.3f) meters = 0.3f;
  float thresh = g_txRef1m - 10.0f * g_pathLossN * log10f(meters);
  return (int8_t)constrain(thresh, -100, -20);
}

float computeCalibratedN() {
  float diff = (float)g_calRssi1m - (float)g_calRssiD2;
  float d = (g_calDist2 > 1.05f) ? g_calDist2 : 1.05f;
  float n = diff / (10.0f * log10f(d));
  if (!isfinite(n)) n = g_pathLossN;
  return constrain(n, 1.2f, 6.0f);
}

const char* menuItemLabel(int idx, char *buf, size_t bufsz) {
  switch (idx) {
    case MI_MODE:
      snprintf(buf, bufsz, "Mode: %s", g_prevOperatingState == ST_RADAR ? "Radar" : "Finder");
      return buf;
    case MI_GAME: return "Reaction Game";
    case MI_CALIBRATE: return "Calibrate...";
    case MI_DEVICES: return "Known Devices";
    case MI_TRUST_ALL:
      snprintf(buf, bufsz, "TrustAllNow(%d)", candidateCountActive());
      return buf;
    case MI_CLEAR_TRUST:
      snprintf(buf, bufsz, "ClearTrust(%d)", g_trusted.count);
      return buf;
    case MI_MUTE:
      snprintf(buf, bufsz, "Mute: %s", alertIsMuted() ? "On" : "Off");
      return buf;
    case MI_RESET_CALIB: return "Reset Calib";
    case MI_EXIT: return "Exit";
  }
  return "?";
}

void handleCalibrationButtons(ButtonEvent ev, ButtonEvent ev2,
                              const int *activeIdx, int activeCount) {
  if (ev == BTN_LONG) { g_state = ST_MENU; return; }
  if (g_calStep == CAL_SELECT1 && ev2 == BTN_SHORT && activeCount > 0) {
    g_calSelectIdx = (g_calSelectIdx + 1) % activeCount;
    return;
  }
  if (g_calStep == CAL_SETDIST2 && ev2 == BTN_SHORT) {
    g_calDist2 += 0.5f;
    if (g_calDist2 > RANGE_MAX_M) g_calDist2 = RANGE_MIN_M;
    return;
  }
  if (ev != BTN_SHORT) return;

  switch (g_calStep) {
    case CAL_SELECT1: {
      if (activeCount == 0) break;   // nothing to pick yet
      if (g_calSelectIdx >= activeCount) g_calSelectIdx = activeCount - 1;
      memcpy(g_calTargetMac, g_candidates[activeIdx[g_calSelectIdx]].mac, 6);
      g_calStep = CAL_CONFIRM1;
      break;
    }
    case CAL_CONFIRM1: {
      int idx = candidateFind(g_calTargetMac);
      if (idx < 0) { g_calNoSignalUntil = millis() + 1500; break; }
      g_calRssiSum = 0; g_calRssiCount = 0; g_calSampleStartMs = millis();
      g_calStep = CAL_SAMPLE1;
      break;
    }
    case CAL_RESULT1:
      g_calStep = CAL_SETDIST2;
      break;
    case CAL_SETDIST2:
      g_calRssiSum = 0; g_calRssiCount = 0; g_calSampleStartMs = millis();
      g_calStep = CAL_SAMPLE2;
      break;
    case CAL_RESULT2:
      g_txRef1m = g_calRssi1m;
      g_pathLossN = computeCalibratedN();
      persistCalibration();
      Serial.printf("Saved calibration: ref=%ddBm N=%.2f\n", g_txRef1m, g_pathLossN);
      g_state = ST_MENU;
      break;
    default: break;   // CAL_SAMPLE1 / CAL_SAMPLE2 auto-advance, button ignored
  }
}

void calibrationTick() {
  if (g_state != ST_CALIBRATE) return;
  if (g_calStep != CAL_SAMPLE1 && g_calStep != CAL_SAMPLE2) return;

  int idx = candidateFind(g_calTargetMac);
  if (idx >= 0) { g_calRssiSum += g_candidates[idx].rssi; g_calRssiCount++; }

  if (millis() - g_calSampleStartMs >= CAL_SAMPLE_MS) {
    if (g_calRssiCount == 0) {
      g_calNoSignalUntil = millis() + 1500;
      g_calStep = (g_calStep == CAL_SAMPLE1) ? CAL_CONFIRM1 : CAL_SETDIST2;
      return;
    }
    int8_t avg = (int8_t)(g_calRssiSum / g_calRssiCount);
    if (g_calStep == CAL_SAMPLE1) { g_calRssi1m = avg; g_calStep = CAL_RESULT1; }
    else { g_calRssiD2 = avg; g_calStep = CAL_RESULT2; }
  }
}

void drawCalibrateScreen(const int *activeIdx, int activeCount) {
  bool noSignal = millis() < g_calNoSignalUntil;
  switch (g_calStep) {
    case CAL_SELECT1:
      if (activeCount == 0) {
        drawTextScreen("CALIBRATE 1/2", "Turn on phone's", "2.4GHz hotspot", "...then select it");
      } else {
        drawDevicePickScreen("SELECT DEVICE", "B1 NEXT  B2 USE",
                              &g_candidates[activeIdx[g_calSelectIdx]], g_calSelectIdx, activeCount);
      }
      break;
    case CAL_CONFIRM1: {
      int idx = candidateFind(g_calTargetMac);
      const char *tag = "device";
      if (idx >= 0) {
        Candidate &c = g_candidates[idx];
        tag = c.label[0] ? c.label : (c.hasName ? c.name : "device");
      }
      char l[24]; snprintf(l, sizeof(l), "Place: %.10s", tag);
      if (noSignal) drawTextScreen("CALIBRATE 1/2", "signal lost!", "move it closer", "B1=retry");
      else drawTextScreen("CALIBRATE 1/2", l, "exactly 1m away", "B1=sample");
      break;
    }
    case CAL_SAMPLE1: {
      char l[24]; snprintf(l, sizeof(l), "reading... (%d)", g_calRssiCount);
      drawTextScreen("CALIBRATE 1/2", "sampling...", l, "hold=cancel");
      break;
    }
    case CAL_RESULT1: {
      char l[24]; snprintf(l, sizeof(l), "1m RSSI=%ddBm", g_calRssi1m);
      drawTextScreen("CALIBRATE 1/2", l, "", "B1=continue");
      break;
    }
    case CAL_SETDIST2: {
      char l[24]; snprintf(l, sizeof(l), "Dist:%.1fm (B2+)", g_calDist2);
      if (noSignal) drawTextScreen("CALIBRATE 2/2", "device lost!", "reposition it", "B1=retry");
      else drawTextScreen("CALIBRATE 2/2", "Move SAME device", l, "B1=sample");
      break;
    }
    case CAL_SAMPLE2: {
      char l[24]; snprintf(l, sizeof(l), "reading... (%d)", g_calRssiCount);
      drawTextScreen("CALIBRATE 2/2", "sampling...", l, "hold=cancel");
      break;
    }
    case CAL_RESULT2: {
      char l1[24];
      snprintf(l1, sizeof(l1), "N=%.1f ref=%ddBm", computeCalibratedN(), g_calRssi1m);
      drawTextScreen("CALIBRATE DONE", l1, "", "B1=save");
      break;
    }
  }
}

int gameNextLane() {
  if (g_gameLaneBagPos >= 3) {
    for (int i = 0; i < 3; i++) g_gameLaneBag[i] = i;
    for (int i = 2; i > 0; i--) {
      int j = (int)(esp_random() % (i + 1));
      int temp = g_gameLaneBag[i];
      g_gameLaneBag[i] = g_gameLaneBag[j];
      g_gameLaneBag[j] = temp;
    }
    g_gameLaneBagPos = 0;
  }
  return g_gameLaneBag[g_gameLaneBagPos++];
}

void gameSpawnObstacle(int slot) {
  int lane = gameNextLane();
  g_gameObstacleLane[slot] = lane;
  g_gameObstacleY[slot] = -8;
  g_gameObstacleAccumMs = 0;
}

void gameStartRound() {
  g_gameRound = 0;
  g_gameScore = 0;
  g_gameLane = 1;
  g_gameLaneBagPos = 3;
  for (int i = 0; i < 3; i++) g_gameObstacleY[i] = -1;
  gameSpawnObstacle(0);
  g_gameFinished = false;
  g_gameStarted = true;
  g_gameLastTickMs = millis();
  g_gameNextObstacleMs = millis() + 900;
}

void gameTick() {
  if (!g_gameStarted || g_gameFinished) return;
  uint32_t now = millis();
  uint32_t dt = now - g_gameLastTickMs;
  g_gameLastTickMs = now;
  g_gameObstacleAccumMs += dt;
  while (g_gameObstacleAccumMs >= 45) {
    for (int i = 0; i < 3; i++) {
      if (g_gameObstacleY[i] >= -8) g_gameObstacleY[i]++;
    }
    g_gameObstacleAccumMs -= 45;
  }

  for (int i = 0; i < 3; i++) {
    if (g_gameObstacleY[i] < -8) continue;
    if (g_gameObstacleLane[i] == g_gameLane && g_gameObstacleY[i] >= 23) {
      g_gameFinished = true;
      return;
    }
    if (g_gameObstacleY[i] > 34) {
      g_gameScore++;
      g_gameRound++;
      g_gameObstacleY[i] = -1;
    }
  }

  if (now >= g_gameNextObstacleMs) {
    for (int i = 0; i < 3; i++) {
      if (g_gameObstacleY[i] < -8) {
        gameSpawnObstacle(i);
        g_gameNextObstacleMs = now + max(280UL, 700UL - g_gameRound * 15UL);
        break;
      }
    }
  }
}

void handleButtons(const int *activeIdx, int activeCount) {
  ButtonEvent ev = buttonGetEvent(g_btnMode);
  ButtonEvent ev2 = BTN_NONE;
#if USE_SELECT_BTN
  ev2 = buttonGetEvent(g_btnSelect);
#endif

  if (g_state == ST_CALIBRATE) {
    handleCalibrationButtons(ev, ev2, activeIdx, activeCount);
    return;
  }

  if (g_state == ST_GAME) {
    if (ev == BTN_LONG) {
      g_state = ST_MENU;
    } else if (ev == BTN_SHORT && (!g_gameStarted || g_gameFinished)) {
      gameStartRound();
    } else if (ev == BTN_SHORT && !g_gameFinished && g_gameLane > 0) {
      g_gameLane--;
    }
#if USE_SELECT_BTN
    if (ev2 == BTN_SHORT && !g_gameFinished && g_gameLane < 2) {
      g_gameLane++;
    }
#endif
    return;
  }

  if (g_state == ST_DEVICE_LIST) {
    if (activeCount > 0 && g_deviceListIdx >= activeCount) g_deviceListIdx = activeCount - 1;
    if (ev == BTN_SHORT && activeCount > 0) {
      g_deviceListIdx = (g_deviceListIdx + 1) % activeCount;
    } else if (ev2 == BTN_SHORT || ev2 == BTN_LONG) {
      if (activeCount > 0) {
        Candidate &c = g_candidates[activeIdx[g_deviceListIdx]];
        if (c.trusted) { trustedRemove(c.mac); c.trusted = false; }
        else { trustedAdd(c.mac); c.trusted = true; }
        candidateRecomputeScore(c);
      }
    } else if (ev == BTN_LONG) {
      g_state = ST_MENU;
    }
    return;
  }

  if (g_state == ST_MENU) {
    if (ev == BTN_SHORT) {
      g_menuIndex = (g_menuIndex + 1) % MI_COUNT;
    } else if (ev2 == BTN_SHORT || ev2 == BTN_LONG) {
      switch (g_menuIndex) {
        case MI_MODE:
          g_prevOperatingState = (g_prevOperatingState == ST_RADAR) ? ST_FINDER : ST_RADAR;
          g_state = g_prevOperatingState;
          break;
        case MI_GAME:
          g_state = ST_GAME;
          g_gameRound = 0;
          g_gameScore = 0;
          g_gameFinished = false;
          g_gameStarted = false;
          for (int i = 0; i < 3; i++) g_gameObstacleY[i] = -1;
          break;
        case MI_CALIBRATE:
          g_state = ST_CALIBRATE;
          g_calStep = CAL_SELECT1;
          g_calSelectIdx = 0;
          g_calNoSignalUntil = 0;
          break;
        case MI_DEVICES:
          g_state = ST_DEVICE_LIST;
          g_deviceListIdx = 0;
          break;
        case MI_TRUST_ALL:
          trustedSnapshotAll();
          break;
        case MI_CLEAR_TRUST:
          trustedClear();
          break;
        case MI_MUTE:
          alertToggleMute();
          break;
        case MI_RESET_CALIB:
          resetCalibrationDefaults();
          break;
        case MI_EXIT:
          g_state = g_prevOperatingState;
          break;
      }
    } else if (ev == BTN_LONG) {
      g_state = g_prevOperatingState;
    }
    return;
  }

  // ST_RADAR or ST_FINDER
  if (ev == BTN_SHORT) {
    g_state = (g_state == ST_RADAR) ? ST_FINDER : ST_RADAR;
    g_finderTargetIdx = -1;
  } else if (ev == BTN_LONG) {
    g_prevOperatingState = g_state;
    g_state = ST_MENU;
  }

#if USE_SELECT_BTN
  if (ev2 == BTN_SHORT) {
    if (g_state == ST_FINDER) {
      int start = (g_finderTargetIdx < 0) ? -1 : g_finderTargetIdx;
      int idx = start;
      for (int tries = 0; tries < MAX_CANDIDATES; tries++) {
        idx = (idx + 1) % MAX_CANDIDATES;
        if (g_candidates[idx].active) { g_finderTargetIdx = idx; break; }
      }
    } else {
      alertToggleMute();
    }
  } else if (ev2 == BTN_LONG) {
    if (g_state == ST_FINDER) {
      int idx = (g_finderTargetIdx >= 0 && g_candidates[g_finderTargetIdx].active)
                  ? g_finderTargetIdx : candidateStrongest(0);
      if (idx >= 0) {
        trustedAdd(g_candidates[idx].mac);
        g_candidates[idx].trusted = true;
        candidateRecomputeScore(g_candidates[idx]);
        g_trustedFlashUntil = millis() + 1200;
      }
    } else {
      alertToggleMute();
    }
  }
#endif
}

void loop() {
  buttonsTick();
  candidateStorePrune(millis());

  static int s_activeIdx[MAX_CANDIDATES];
  int s_activeCount = candidateBuildActiveList(s_activeIdx, MAX_CANDIDATES);

  handleButtons(s_activeIdx, s_activeCount);

  bool finderLocked = false;
  uint8_t finderChannel = 0;

  switch (g_state) {
    case ST_FINDER: {
      if (millis() < g_trustedFlashUntil) {
        alertSetPattern(ALERT_OFF);
        drawTextScreen("FINDER", "Device marked", "as TRUSTED", "");
        break;
      }
      int idx = (g_finderTargetIdx >= 0 && g_candidates[g_finderTargetIdx].active)
                  ? g_finderTargetIdx
                  : candidateStrongest(SCORE_FLAG_THRESHOLD, true);
      bool unconfirmed = false;
      if (idx < 0) { idx = candidateStrongest(0); unconfirmed = true; }

      if (idx >= 0) {
        finderLocked = true;
        finderChannel = g_candidates[idx].channel;
        int8_t rssi = g_candidates[idx].rssi;
        if (g_candidates[idx].trusted) {
          alertSetPattern(ALERT_OFF);
          alertSetLedBrightness(0);
        } else {
          uint32_t interval = map(constrain(rssi, -90, -40), -90, -40, 900, 100);
          alertSetPattern(ALERT_PERIODIC, rssi >= -35 ? 180 : interval);
          uint8_t brightness = map(constrain(rssi, -90, -40), -90, -40, 40, 255);
          alertSetLedBrightness(brightness);
        }
        int posIdx = 0;
        for (int k = 0; k < s_activeCount; k++) if (s_activeIdx[k] == idx) { posIdx = k; break; }
        drawFinderScreen(true, &g_candidates[idx], unconfirmed, alertIsMuted(), posIdx, s_activeCount);
      } else {
        alertSetPattern(ALERT_OFF);
        drawFinderScreen(false, nullptr, false, alertIsMuted(), 0, 0);
      }
      break;
    }

    case ST_MENU: {
      alertSetPattern(ALERT_OFF);
      char buf[20];
      const char *label = menuItemLabel(g_menuIndex, buf, sizeof(buf));
      drawMenuScreen("MENU", label, g_menuIndex, MI_COUNT);
      break;
    }

    case ST_DEVICE_LIST: {
      alertSetPattern(ALERT_OFF);
      if (s_activeCount == 0) {
        drawTextScreen("KNOWN DEVICES", "no devices", "detected yet", "hold=back");
      } else {
        const Candidate &c = g_candidates[s_activeIdx[g_deviceListIdx]];
        drawDevicePickScreen("KNOWN DEVICES", c.trusted ? "B2 UNTRUST  B1 NEXT" : "B2 TRUST  B1 NEXT",
                              &c, g_deviceListIdx, s_activeCount);
      }
      break;
    }

    case ST_CALIBRATE: {
      calibrationTick();
      alertSetPattern(ALERT_OFF);
      drawCalibrateScreen(s_activeIdx, s_activeCount);
      break;
    }

    case ST_GAME: {
      gameTick();
      alertSetPattern(ALERT_OFF);
      drawGameScreen(g_gameLane, g_gameObstacleLane, g_gameObstacleY,
                     g_gameScore, g_gameStarted, g_gameFinished);
      break;
    }

    case ST_RADAR:
    default: {
      float rangeM = currentRangeMeters();
      int8_t thresh = rssiThresholdForRange(rangeM);

      uint32_t now = millis();
      uint32_t dt = now - g_lastSweepUpdateMs;
      g_lastSweepUpdateMs = now;
      float prevAngle = g_sweepAngle;
      g_sweepAngle += (TWO_PI * dt) / (float)SWEEP_PERIOD_MS;
      bool wrapped = false;
      if (g_sweepAngle >= TWO_PI) { g_sweepAngle -= TWO_PI; wrapped = true; }

      int flaggedInRange = 0, totalActive = 0, untrustedActive = 0;
      bool pinged = false; int pingRssi = -100; float pingContactAngle = -1.0f;
      uint8_t newMac[6]; int8_t newRssi = -100;
      bool newDevice = candidateTakeNewEvent(newMac, newRssi);
      if (newDevice) {
        int newIdx = candidateFind(newMac);
        if (newIdx >= 0 && !g_candidates[newIdx].trusted) {
          pinged = true;
          pingRssi = newRssi;
          pingContactAngle = macToAngle(newMac);
        } else {
          newDevice = false;
        }
      }
      for (int i = 0; i < MAX_CANDIDATES; i++) {
        if (!g_candidates[i].active) continue;
        totalActive++;
        if (!g_candidates[i].trusted) untrustedActive++;
        bool inRangeFlagged = (g_candidates[i].score >= SCORE_FLAG_THRESHOLD &&
                                g_candidates[i].rssi >= thresh && !g_candidates[i].trusted);
        if (inRangeFlagged) {
          flaggedInRange++;
          float a = macToAngle(g_candidates[i].mac);
          bool crossed = wrapped ? (a >= prevAngle || a <= g_sweepAngle)
                                  : (a >= prevAngle && a <= g_sweepAngle);
          if (crossed) {
            pinged = true;
            if (g_candidates[i].rssi > pingRssi) {
              pingRssi = g_candidates[i].rssi;
              pingContactAngle = a;
            }
          }
        }
      }

      alertSetPattern(ALERT_OFF);   // Radar only makes sound via sweep pings
      if (pinged) {
        int freq = map(constrain(pingRssi, -90, -40), -90, -40, 1600, 3200);
        alertFirePing(freq, 255);
      }

      drawRadarScreen(g_sweepAngle, rangeM, flaggedInRange, totalActive, untrustedActive,
                      candidateAllTrusted(), alertIsMuted(), pinged, newDevice,
                      pingContactAngle);
      break;
    }
  }

  wifiScannerTick(finderLocked, finderChannel);
  bleScannerTick();
  alertTick();
}
