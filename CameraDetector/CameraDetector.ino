// ============================================================
//  WiFi / BLE Camera & All-Device RF Detector  (v4)
//  ESP32-C3 or ESP32-S3; hardware profile auto-selected by board target
//
//  RADAR   - sweeping radar; beeps + LED flash on target contact
//  FINDER  - pinpoint proximity tracker with peak hold & geiger audio
//  GAME    - retro arcade minigame ("SPY EVADER") with sound FX & high scores
//
//  Controls:
//  Button 1 short  -> switch RADAR <-> FINDER (or Move Left in Game)
//  Button 1 long   -> open / exit MENU
//  Button 2 short  -> cycle SENSITIVITY in Radar / next target in Finder / Move Right in Game / Select in Menu
//  Button 2 long   -> toggle Mute in Radar / Trust target in Finder
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
static AppState g_prevOperatingState = ST_RADAR;

enum MenuItem {
  MI_MODE = 0,
  MI_TARGET_FILTER,
  MI_SENSITIVITY,
  MI_VOLUME,
  MI_GAME,
  MI_DEVICES,
  MI_TRUST_ALL,
  MI_CLEAR_TRUST,
  MI_CALIBRATE,
  MI_RESET_SETTINGS,
  MI_EXIT,
  MI_COUNT
};
static int g_menuIndex = 0;
static int g_deviceListIdx = 0;

// Detection & Sensitivity modes
static TargetFilter    g_targetFilter = TARGET_CAM_ONLY;
static SensitivityMode g_sensMode     = SENS_MEDIUM;

// Finder peak RSSI & tracking
static int8_t   g_finderPeakRssi = -100;
static int      g_finderLastTargetIdx = -1;
static uint32_t g_lastGeigerTickMs = 0;
static uint32_t g_trustedFlashUntil = 0;
static bool     g_trustedFlashWasTrust = true;

// Minigame ("Spy Evader") State
#define MAX_GAME_ENTITIES 4
static GameEntity g_gameEntities[MAX_GAME_ENTITIES];
static int      g_gameScore = 0;
static int      g_gameHighScore = 0;
static int      g_gameLane = 1;
static int      g_gameLaneDir = 1;   // for single-button lane cycling
static bool     g_gameStarted = false;
static bool     g_gameFinished = false;
static bool     g_gameNewHighScore = false;
static uint32_t g_gameLastTickMs = 0;
static uint32_t g_gameNextSpawnMs = 0;
static uint32_t g_gameEntityAccumMs = 0;
static uint8_t  g_gameRoadAnim = 0;
static uint32_t g_gameSpeedStepMs = 40;

// Calibration Wizard State
enum CalibStep {
  CAL_SELECT1,
  CAL_CONFIRM1,
  CAL_SAMPLE1,
  CAL_RESULT1,
  CAL_SETDIST2,
  CAL_SAMPLE2,
  CAL_RESULT2
};
static CalibStep g_calStep = CAL_SELECT1;
static int       g_calSelectIdx = 0;
static uint8_t   g_calTargetMac[6];
static float     g_calRssiSum = 0;
static int       g_calRssiCount = 0;
static uint32_t  g_calSampleStartMs = 0;
static int8_t    g_calRssi1m = TX_REF_RSSI_1M;
static float     g_calDist2 = 2.0f;
static int8_t    g_calRssiD2 = TX_REF_RSSI_1M;
static uint32_t  g_calNoSignalUntil = 0;

static int g_btnMode = -1;
static int g_btnSelect = -1;
static int g_finderTargetIdx = -1;

// Runtime calibration values (persisted via Preferences)
static int   g_txRef1m   = TX_REF_RSSI_1M;
static float g_pathLossN = PATH_LOSS_N;

static float    g_sweepAngle = 0;
static uint32_t g_lastSweepUpdateMs = 0;

Preferences g_prefs;

void loadSettings() {
  g_prefs.begin("camdet", true);
  g_txRef1m       = g_prefs.getInt("txref", TX_REF_RSSI_1M);
  g_pathLossN     = g_prefs.getFloat("pathn", PATH_LOSS_N);
  g_targetFilter  = (TargetFilter)g_prefs.getInt("filter", (int)TARGET_CAM_ONLY);
  g_sensMode      = (SensitivityMode)g_prefs.getInt("sens", (int)SENS_MEDIUM);
  if ((int)g_sensMode >= SENS_COUNT) g_sensMode = SENS_MEDIUM;
  int vol = g_prefs.getInt("vol", (int)VOL_HIGH);
  alertSetVolume((VolumeLevel)vol);
  g_gameHighScore = g_prefs.getInt("highscore", 0);
  g_prefs.end();
}

void persistSettings() {
  g_prefs.begin("camdet", false);
  g_prefs.putInt("txref", g_txRef1m);
  g_prefs.putFloat("pathn", g_pathLossN);
  g_prefs.putInt("filter", (int)g_targetFilter);
  g_prefs.putInt("sens", (int)g_sensMode);
  g_prefs.putInt("vol", (int)alertGetVolume());
  g_prefs.putInt("highscore", g_gameHighScore);
  g_prefs.end();
}

void saveHighScore(int hs) {
  g_gameHighScore = hs;
  g_prefs.begin("camdet", false);
  g_prefs.putInt("highscore", g_gameHighScore);
  g_prefs.end();
}

void resetSettingsDefaults() {
  g_txRef1m = TX_REF_RSSI_1M;
  g_pathLossN = PATH_LOSS_N;
  g_targetFilter = TARGET_CAM_ONLY;
  g_sensMode = SENS_MEDIUM;
  alertSetVolume(VOL_HIGH);
  persistSettings();
}

void setup() {
  Serial.begin(115200);
  delay(200);
  Serial.println("\n[boot] Cyber-Detect RF Sweeper v4 starting...");
  randomSeed(esp_random() ^ micros());

  Serial.println("[boot] loading settings");
  loadSettings();
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

  displaySplash();
  Serial.println("[boot] initializing WiFi sniffer");
  wifiScannerInit();
  Serial.println("[boot] initializing BLE scanner");
  bleScannerInit();
  g_lastSweepUpdateMs = millis();

  Serial.printf("[ready] ref=%ddBm N=%.2f filter=%d sens=%d trusted=%d highscore=%d\n",
                g_txRef1m, g_pathLossN, (int)g_targetFilter, (int)g_sensMode, g_trusted.count, g_gameHighScore);
}

int8_t getActiveThreshold(float &rangeMetersOut) {
  switch (g_sensMode) {
    case SENS_PINPOINT:
      rangeMetersOut = 0.4f;
      return SENS_PINPOINT_RSSI;   // -48 dBm
    case SENS_HIGH:
      rangeMetersOut = 1.5f;
      return SENS_HIGH_RSSI;       // -62 dBm
    case SENS_MEDIUM:
      rangeMetersOut = 4.0f;
      return SENS_MEDIUM_RSSI;     // -74 dBm
    case SENS_LOW:
    default:
      rangeMetersOut = 12.0f;
      return SENS_LOW_RSSI;        // -86 dBm
  }
}

float computeCalibratedN() {
  float diff = (float)g_calRssi1m - (float)g_calRssiD2;
  float d = (g_calDist2 > 1.05f) ? g_calDist2 : 1.05f;
  float n = diff / (10.0f * log10f(d));
  if (!isfinite(n)) n = g_pathLossN;
  return constrain(n, 1.2f, 6.0f);
}

void getMenuItem(int idx, char *titleBuf, size_t titleSz, char *valBuf, size_t valSz) {
  switch (idx) {
    case MI_MODE:
      snprintf(titleBuf, titleSz, "OPERATING MODE");
      snprintf(valBuf, valSz, "%s", g_prevOperatingState == ST_RADAR ? "RADAR" : "FINDER");
      break;
    case MI_TARGET_FILTER:
      snprintf(titleBuf, titleSz, "TARGET FILTER");
      snprintf(valBuf, valSz, "%s", g_targetFilter == TARGET_ALL_DEV ? "ALL DEVICES" : "CAMERA ONLY");
      break;
    case MI_SENSITIVITY: {
      snprintf(titleBuf, titleSz, "SENSITIVITY");
      const char *s = (g_sensMode == SENS_PINPOINT) ? "PINPOINT" :
                      (g_sensMode == SENS_HIGH) ? "HIGH (1.5m)" :
                      (g_sensMode == SENS_MEDIUM) ? "MED (4.0m)" : "LOW (12m)";
      snprintf(valBuf, valSz, "%s", s);
      break;
    }
    case MI_VOLUME:
      snprintf(titleBuf, titleSz, "AUDIO VOLUME");
      snprintf(valBuf, valSz, "%s", alertVolumeLabel(alertGetVolume()));
      break;
    case MI_GAME:
      snprintf(titleBuf, titleSz, "SPY EVADER");
      snprintf(valBuf, valSz, "LAUNCH GAME");
      break;
    case MI_DEVICES:
      snprintf(titleBuf, titleSz, "KNOWN DEVICES");
      snprintf(valBuf, valSz, "%02d DETECTED", candidateCountActive());
      break;
    case MI_TRUST_ALL:
      snprintf(titleBuf, titleSz, "TRUST ALL NOW");
      snprintf(valBuf, valSz, "ADD %02d ACTIVE", candidateCountActive());
      break;
    case MI_CLEAR_TRUST:
      snprintf(titleBuf, titleSz, "CLEAR TRUST");
      snprintf(valBuf, valSz, "RESET %02d SAVED", g_trusted.count);
      break;
    case MI_CALIBRATE:
      snprintf(titleBuf, titleSz, "CALIBRATION");
      snprintf(valBuf, valSz, "START WIZARD");
      break;
    case MI_RESET_SETTINGS:
      snprintf(titleBuf, titleSz, "RESET DEFAULTS");
      snprintf(valBuf, valSz, "RESTORE");
      break;
    case MI_EXIT:
      snprintf(titleBuf, titleSz, "EXIT MENU");
      snprintf(valBuf, valSz, "RETURN >>>");
      break;
    default:
      snprintf(titleBuf, titleSz, "?");
      snprintf(valBuf, valSz, "-");
      break;
  }
}


void handleCalibrationButtons(ButtonEvent ev, ButtonEvent ev2,
                              const int *activeIdx, int activeCount) {
  if (ev == BTN_LONG) { g_state = ST_MENU; return; }
  if (g_calStep == CAL_SELECT1 && ev2 == BTN_SHORT && activeCount > 0) {
    g_calSelectIdx = (g_calSelectIdx + 1) % activeCount;
    return;
  }
  if (g_calStep == CAL_SETDIST2 && ev2 == BTN_SHORT) {
    g_calDist2 += 1.0f;
    if (g_calDist2 > 8.0f) g_calDist2 = 2.0f;
    return;
  }
  if (ev != BTN_SHORT) return;

  switch (g_calStep) {
    case CAL_SELECT1: {
      if (activeCount == 0) break;
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
      persistSettings();
      Serial.printf("Saved calibration: ref=%ddBm N=%.2f\n", g_txRef1m, g_pathLossN);
      g_state = ST_MENU;
      break;
    default: break;
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

// ------------------------------------------------------------
//  Spy Evader Arcade Minigame
// ------------------------------------------------------------

void gameSpawnEntity() {
  int slot = -1;
  for (int i = 0; i < MAX_GAME_ENTITIES; i++) {
    if (!g_gameEntities[i].active) { slot = i; break; }
  }
  if (slot < 0) return;

  int lane = esp_random() % 3;
  uint8_t type = (esp_random() % 4 == 0) ? 1 : 0;

  g_gameEntities[slot].lane = (int8_t)lane;
  g_gameEntities[slot].y = -4;
  g_gameEntities[slot].type = type;
  g_gameEntities[slot].active = true;
}

void gameStartRound() {
  for (int i = 0; i < MAX_GAME_ENTITIES; i++) g_gameEntities[i].active = false;
  g_gameScore = 0;
  g_gameLane = 1;
  g_gameLaneDir = 1;
  g_gameFinished = false;
  g_gameStarted = true;
  g_gameNewHighScore = false;
  g_gameSpeedStepMs = 40;
  g_gameEntityAccumMs = 0;
  g_gameLastTickMs = millis();
  g_gameNextSpawnMs = millis() + 500;
  gameSpawnEntity();
  alertSfxSteer();
}

void gameTick() {
  if (!g_gameStarted || g_gameFinished) return;
  uint32_t now = millis();
  uint32_t dt = now - g_gameLastTickMs;
  g_gameLastTickMs = now;
  g_gameEntityAccumMs += dt;

  while (g_gameEntityAccumMs >= g_gameSpeedStepMs) {
    g_gameRoadAnim = (g_gameRoadAnim + 1) % 6;
    for (int i = 0; i < MAX_GAME_ENTITIES; i++) {
      if (g_gameEntities[i].active) {
        g_gameEntities[i].y++;
      }
    }
    g_gameEntityAccumMs -= g_gameSpeedStepMs;
  }


  int playerHitYMin = OLED_H - 12;
  int playerHitYMax = OLED_H - 3;

  for (int i = 0; i < MAX_GAME_ENTITIES; i++) {
    if (!g_gameEntities[i].active) continue;

    // Check collision with player
    if (g_gameEntities[i].lane == g_gameLane &&
        g_gameEntities[i].y >= playerHitYMin &&
        g_gameEntities[i].y <= playerHitYMax) {
      if (g_gameEntities[i].type == 0) {
        // Collided with obstacle -> Game Over!
        g_gameFinished = true;
        alertSfxCrash();
        if (g_gameScore > g_gameHighScore) {
          g_gameNewHighScore = true;
          saveHighScore(g_gameScore);
          alertSfxHighScore();
        }
        return;
      } else {
        // Collected bonus data orb! (+5 points)
        g_gameScore += 5;
        alertSfxCoin();
        g_gameEntities[i].active = false;
      }
    }

    // Entity passed off bottom of screen
    if (g_gameEntities[i].y > OLED_H + 4) {
      if (g_gameEntities[i].type == 0) {
        g_gameScore += 1;
        // Speed ramps up as score increases
        g_gameSpeedStepMs = max(16UL, 40UL - (uint32_t)(g_gameScore / 4));
      }
      g_gameEntities[i].active = false;
    }
  }

  // Periodic spawning
  if (now >= g_gameNextSpawnMs) {
    gameSpawnEntity();
    uint32_t spawnDelay = max(260UL, 650UL - (uint32_t)(g_gameScore * 8));
    g_gameNextSpawnMs = now + spawnDelay;
  }
}

// ------------------------------------------------------------
//  Button & Input Handling (Optimized for 2-button / 1-button / Pot setups)
// ------------------------------------------------------------

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
    } else if (ev == BTN_SHORT) {
      if (!g_gameStarted || g_gameFinished) {
        gameStartRound();
      } else {
#if USE_SELECT_BTN
        // In 2-button setup: Button 1 = Left
        if (g_gameLane > 0) { g_gameLane--; alertSfxSteer(); }
#else
        // In 1-button setup: Cycle lanes 0 -> 1 -> 2 -> 1 -> 0
        g_gameLane += g_gameLaneDir;
        if (g_gameLane >= 2) { g_gameLane = 2; g_gameLaneDir = -1; }
        else if (g_gameLane <= 0) { g_gameLane = 0; g_gameLaneDir = 1; }
        alertSfxSteer();
#endif
      }
    }
#if USE_SELECT_BTN
    if (ev2 == BTN_SHORT) {
      if (!g_gameStarted || g_gameFinished) {
        gameStartRound();
      } else {
        // Button 2 = Right
        if (g_gameLane < 2) { g_gameLane++; alertSfxSteer(); }
      }
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
      alertPlaySfx((const SfxNote[]){ {2000, 15} }, 1);
    } else if (ev2 == BTN_SHORT || ev2 == BTN_LONG) {
      switch (g_menuIndex) {
        case MI_MODE:
          g_prevOperatingState = (g_prevOperatingState == ST_RADAR) ? ST_FINDER : ST_RADAR;
          g_state = g_prevOperatingState;
          alertPlaySfx((const SfxNote[]){ {2400, 30} }, 1);
          break;
        case MI_TARGET_FILTER:
          g_targetFilter = (g_targetFilter == TARGET_CAM_ONLY) ? TARGET_ALL_DEV : TARGET_CAM_ONLY;
          persistSettings();
          alertPlaySfx((const SfxNote[]){ {2400, 35} }, 1);
          break;
        case MI_SENSITIVITY:
          g_sensMode = (SensitivityMode)(((int)g_sensMode + 1) % SENS_COUNT);
          persistSettings();
          alertPlaySfx((const SfxNote[]){ {2600, 35} }, 1);
          break;
        case MI_VOLUME:
          alertCycleVolume();
          persistSettings();
          if (alertGetVolume() != VOL_MUTE) {
            alertPlaySfx((const SfxNote[]){ {2400, 35} }, 1);
          }
          break;
        case MI_GAME:
          g_state = ST_GAME;
          g_gameStarted = false;
          g_gameFinished = false;
          g_gameScore = 0;
          for (int i = 0; i < MAX_GAME_ENTITIES; i++) g_gameEntities[i].active = false;
          break;
        case MI_DEVICES:
          g_state = ST_DEVICE_LIST;
          g_deviceListIdx = 0;
          alertPlaySfx((const SfxNote[]){ {2200, 30} }, 1);
          break;
        case MI_TRUST_ALL:
          trustedSnapshotAll();
          alertPlaySfx((const SfxNote[]){ {2200, 40}, {2800, 60} }, 2);
          break;
        case MI_CLEAR_TRUST:
          trustedClear();
          alertPlaySfx((const SfxNote[]){ {1400, 40} }, 1);
          break;
        case MI_CALIBRATE:
          g_state = ST_CALIBRATE;
          g_calStep = CAL_SELECT1;
          g_calSelectIdx = 0;
          g_calNoSignalUntil = 0;
          break;
        case MI_RESET_SETTINGS:
          resetSettingsDefaults();
          alertPlaySfx((const SfxNote[]){ {1200, 40}, {1600, 40} }, 2);
          break;
        case MI_EXIT:
          g_state = g_prevOperatingState;
          alertPlaySfx((const SfxNote[]){ {1800, 30} }, 1);
          break;
      }
    } else if (ev == BTN_LONG) {
      g_state = g_prevOperatingState;
      alertPlaySfx((const SfxNote[]){ {1800, 30} }, 1);
    }
    return;
  }

  // ST_RADAR or ST_FINDER
  if (ev == BTN_SHORT) {
    g_state = (g_state == ST_RADAR) ? ST_FINDER : ST_RADAR;
    g_finderTargetIdx = -1;
    g_finderPeakRssi = -100;
    alertPlaySfx((const SfxNote[]){ {2400, 25} }, 1);
  } else if (ev == BTN_LONG) {
    g_prevOperatingState = g_state;
    g_state = ST_MENU;
    alertPlaySfx((const SfxNote[]){ {2000, 30} }, 1);
  }

#if USE_SELECT_BTN
  if (ev2 == BTN_SHORT) {
    if (g_state == ST_FINDER) {
      // Cycle target candidate
      int start = (g_finderTargetIdx < 0) ? -1 : g_finderTargetIdx;
      int idx = start;
      for (int tries = 0; tries < MAX_CANDIDATES; tries++) {
        idx = (idx + 1) % MAX_CANDIDATES;
        if (g_candidates[idx].active) {
          g_finderTargetIdx = idx;
          g_finderPeakRssi = g_candidates[idx].rssi;
          alertPlaySfx((const SfxNote[]){ {2200, 25} }, 1);
          break;
        }
      }
    } else {
      // In Radar: Button 2 short cycles Sensitivity mode directly (Pinpoint -> High -> Med -> Low)
      g_sensMode = (SensitivityMode)(((int)g_sensMode + 1) % SENS_COUNT);
      persistSettings();
      alertPlaySfx((const SfxNote[]){ {2600, 30} }, 1);
    }
  } else if (ev2 == BTN_LONG) {
    if (g_state == ST_FINDER) {
      int idx = (g_finderTargetIdx >= 0 && g_candidates[g_finderTargetIdx].active)
                  ? g_finderTargetIdx : candidateStrongestFilter(g_targetFilter, false);
      if (idx >= 0) {
        if (g_candidates[idx].trusted) {
          // Untrust target
          trustedRemove(g_candidates[idx].mac);
          g_candidates[idx].trusted = false;
          candidateRecomputeScore(g_candidates[idx]);
          g_trustedFlashWasTrust = false;
          g_trustedFlashUntil = millis() + 1200;
          alertPlaySfx((const SfxNote[]){ {1600, 40}, {1200, 50} }, 2);
        } else {
          // Trust target
          trustedAdd(g_candidates[idx].mac);
          g_candidates[idx].trusted = true;
          candidateRecomputeScore(g_candidates[idx]);
          g_trustedFlashWasTrust = true;
          g_trustedFlashUntil = millis() + 1200;
          alertPlaySfx((const SfxNote[]){ {2200, 40}, {2800, 60} }, 2);
        }
      }
    } else {
      // In Radar: Button 2 long toggles Mute
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
        drawTextScreen("FINDER", "DEVICE MARKED", g_trustedFlashWasTrust ? "AS TRUSTED *" : "AS UNTRUSTED", "");
        break;
      }

      int idx = (g_finderTargetIdx >= 0 && g_candidates[g_finderTargetIdx].active)
                  ? g_finderTargetIdx
                  : candidateStrongestFilter(g_targetFilter, true);
      bool unconfirmed = false;
      if (idx < 0) {
        idx = candidateStrongest(0, false);
        unconfirmed = true;
      }

      if (idx >= 0) {
        finderLocked = true;
        finderChannel = g_candidates[idx].channel;
        int8_t rssi = g_candidates[idx].rssi;

        // Reset peak if target changed
        if (idx != g_finderLastTargetIdx) {
          g_finderPeakRssi = rssi;
          g_finderLastTargetIdx = idx;
        } else if (rssi > g_finderPeakRssi) {
          g_finderPeakRssi = rssi;
        }

        if (g_candidates[idx].trusted) {
          alertSetPattern(ALERT_OFF);
          alertSetLedBrightness(0);
        } else if (g_sensMode == SENS_PINPOINT) {
          // Pinpoint mode: Geiger-counter style rapid clicks as you get closer
          alertSetPattern(ALERT_OFF);
          if (rssi >= -65) {
            uint32_t geigerInterval = map(constrain(rssi, -65, -30), -65, -30, 320, 25);
            if (millis() - g_lastGeigerTickMs >= geigerInterval) {
              alertSfxGeigerTick();
              g_lastGeigerTickMs = millis();
            }
          }
          uint8_t brightness = map(constrain(rssi, -65, -30), -65, -30, 50, 255);
          alertSetLedBrightness(brightness);
        } else {
          // Standard / High / Med / Low finder audio
          uint32_t interval = map(constrain(rssi, -90, -40), -90, -40, 900, 100);
          alertSetPattern(ALERT_PERIODIC, rssi >= -35 ? 160 : interval);
          uint8_t brightness = map(constrain(rssi, -90, -40), -90, -40, 40, 255);
          alertSetLedBrightness(brightness);
        }

        int posIdx = 0;
        for (int k = 0; k < s_activeCount; k++) {
          if (s_activeIdx[k] == idx) { posIdx = k; break; }
        }
        drawFinderScreen(true, &g_candidates[idx], unconfirmed, alertIsMuted(), posIdx, s_activeCount,
                         g_targetFilter, g_sensMode, g_finderPeakRssi);
      } else {
        alertSetPattern(ALERT_OFF);
        drawFinderScreen(false, nullptr, false, alertIsMuted(), 0, 0, g_targetFilter, g_sensMode, -100);
      }
      break;
    }

    case ST_MENU: {
      alertSetPattern(ALERT_OFF);
      char titleBuf[24];
      char valBuf[24];
      getMenuItem(g_menuIndex, titleBuf, sizeof(titleBuf), valBuf, sizeof(valBuf));
      drawMenuScreen("MENU", titleBuf, valBuf, g_menuIndex, MI_COUNT);
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
      drawGameScreen(g_gameLane, g_gameEntities, MAX_GAME_ENTITIES,
                     g_gameScore, g_gameHighScore, g_gameStarted, g_gameFinished,
                     g_gameNewHighScore, g_gameRoadAnim);
      break;
    }

    case ST_RADAR:
    default: {
      float rangeM = 0;
      int8_t thresh = getActiveThreshold(rangeM);

      uint32_t now = millis();
      uint32_t dt = now - g_lastSweepUpdateMs;
      g_lastSweepUpdateMs = now;
      float prevAngle = g_sweepAngle;
      g_sweepAngle += (TWO_PI * dt) / (float)SWEEP_PERIOD_MS;
      bool wrapped = false;
      if (g_sweepAngle >= TWO_PI) { g_sweepAngle -= TWO_PI; wrapped = true; }

      int targetsInRange = 0, totalActive = 0, untrustedActive = 0;
      bool pinged = false; int pingRssi = -100; float pingContactAngle = -1.0f;
      uint8_t newMac[6]; int8_t newRssi = -100;
      bool newDevice = candidateTakeNewEvent(newMac, newRssi);

      if (newDevice) {
        int newIdx = candidateFind(newMac);
        if (newIdx >= 0 && !g_candidates[newIdx].trusted) {
          if (g_targetFilter == TARGET_ALL_DEV || g_candidates[newIdx].score >= SCORE_FLAG_THRESHOLD) {
            pinged = true;
            pingRssi = newRssi;
            pingContactAngle = macToAngle(newMac);
            // Buzz / tone alert when new device detected!
            alertSfxNewDevice();
          }
        } else {
          newDevice = false;
        }
      }

      for (int i = 0; i < MAX_CANDIDATES; i++) {
        if (!g_candidates[i].active) continue;
        totalActive++;
        if (!g_candidates[i].trusted) untrustedActive++;

        bool isTarget = false;
        if (g_targetFilter == TARGET_ALL_DEV) {
          isTarget = (!g_candidates[i].trusted && g_candidates[i].rssi >= thresh);
        } else {
          isTarget = (!g_candidates[i].trusted &&
                      g_candidates[i].score >= SCORE_FLAG_THRESHOLD &&
                      g_candidates[i].rssi >= thresh);
        }

        if (isTarget) {
          targetsInRange++;
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

      alertSetPattern(ALERT_OFF);
      if (pinged) {
        int freq = map(constrain(pingRssi, -90, -40), -90, -40, 1600, 3200);
        alertFirePing(freq, 255);
      }

      drawRadarScreen(g_sweepAngle, rangeM, targetsInRange, totalActive, untrustedActive,
                      candidateAllTrusted(), alertIsMuted(), pinged, newDevice,
                      g_targetFilter, g_sensMode, pingContactAngle);
      break;
    }
  }

  wifiScannerTick(finderLocked, finderChannel);
  bleScannerTick();
  alertTick();
}

