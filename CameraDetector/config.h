#pragma once
#include <Arduino.h>

// ============================================================
//  Camera / Spy-Cam Detector - pin map & tunables
//  Board: ESP32-C3 or ESP32-S3 (selected automatically by the board target)
// ============================================================

#if defined(CONFIG_IDF_TARGET_ESP32C3)
// ---- ESP32-C3 0.42" 72x40 OLED -----------------------------
#define PIN_OLED_SDA   5
#define PIN_OLED_SCL   6
#define OLED_I2C_ADDR  0x3C
#define OLED_W         72
#define OLED_H         40
#define OLED_X_OFFSET  30
#define OLED_Y_OFFSET  12
#define OLED_DRIVER_128X32 0
#define SPLASH_X1 6
#define SPLASH_X2 10
#define SPLASH_Y1 16
#define SPLASH_Y2 28
#define RADAR_CX 36
#define RADAR_CY 21
#define RADAR_MAX_R 12
#define SCREEN_HEADER_Y 7
#define SCREEN_MID_Y 21
#define SCREEN_DBM_Y 30
#define SCREEN_FOOTER_Y 37
#define FINDER_BAR_Y 13
#define FINDER_BAR_W 64
#define FINDER_BAR_H 9
#define PICK_LINE1_Y 18
#define PICK_LINE2_Y 28
#define MENU_BOX_Y 14
#define MENU_BOX_H 12
#define MENU_TEXT_OFFSET  -3
#define TEXT_Y0 8
#define TEXT_Y1 18
#define TEXT_Y2 28
#define GAME_TITLE_X 22
#define GAME_TARGET_X 57
#define GAME_TARGET_Y 22
#define GAME_TARGET_R 5
#define PIN_BTN_MODE   9
#define PIN_BTN_SELECT 0
#define PIN_BUZZER     10
#define PIN_LED        3
#define LED_IS_RGB     0
#else
// ---- ESP32-S3 0.91" 128x32 OLED (I2C) ----------------------
#define PIN_OLED_SDA   41
#define PIN_OLED_SCL   42
#define OLED_I2C_ADDR  0x3C
#define OLED_W         128
#define OLED_H         32
#define OLED_X_OFFSET  0
#define OLED_Y_OFFSET  0
#define OLED_DRIVER_128X32 1
#define SPLASH_X1 44
#define SPLASH_X2 46
#define SPLASH_Y1 14
#define SPLASH_Y2 26
#define RADAR_CX 105
#define RADAR_CY 17
#define RADAR_MAX_R 14
#define SCREEN_HEADER_Y 6
#define SCREEN_MID_Y 17
#define SCREEN_DBM_Y 19
#define SCREEN_FOOTER_Y 30
#define FINDER_BAR_Y 8
#define FINDER_BAR_W 96
#define FINDER_BAR_H 7
#define PICK_LINE1_Y 14
#define PICK_LINE2_Y 22
#define MENU_BOX_Y 8
#define MENU_BOX_H 10
#define MENU_TEXT_OFFSET  -2
#define TEXT_Y0 6
#define TEXT_Y1 14
#define TEXT_Y2 22
#define GAME_TITLE_X 49
#define GAME_TARGET_X 64
#define GAME_TARGET_Y 20
#define GAME_TARGET_R 4
#define PIN_BTN_MODE   16
#define PIN_BTN_SELECT 47
#define PIN_BUZZER     21
#define PIN_LED        48
#define LED_IS_RGB     1
#endif

// ---- Target Filter (Detection Mode) -------------------------------
enum TargetFilter {
  TARGET_CAM_ONLY = 0,   // Camera / surveillance devices only (heuristic score)
  TARGET_ALL_DEV = 1     // Any RF device (WiFi APs, Stations, BLE beacons)
};

// ---- Sensitivity Modes (2-Button Controlled) --------------------
enum SensitivityMode {
  SENS_PINPOINT = 0,  // Ultra close-range sniffing (<0.5m, ~-48dBm threshold)
  SENS_HIGH = 1,      // Close-range (~1.5m, ~-62dBm threshold)
  SENS_MEDIUM = 2,    // Room-wide (~4.0m, ~-74dBm threshold)
  SENS_LOW = 3,       // Perimeter / Far (~12.0m, ~-86dBm threshold)
  SENS_COUNT = 4
};

#define SENS_PINPOINT_RSSI  -48
#define SENS_HIGH_RSSI      -62
#define SENS_MEDIUM_RSSI    -74
#define SENS_LOW_RSSI       -86

// ---- Buttons (2-Button Configuration) ----------------------------
#define USE_SELECT_BTN  1      // 1 = 2-button control (Button 1: Mode/Left, Button 2: Select/Right)

// ---- Buzzer + LED -----------------------------------------------
#define BEEP_FREQ_HZ    3000
#define BEEP_ON_MS      70

// ---- RSSI <-> distance model (defaults; calibrated via Wizard) ---
#define TX_REF_RSSI_1M  -40    // typical RSSI at 1m for WiFi/BLE
#define PATH_LOSS_N     2.5f   // 2=free space, 2.5-4=indoor w/ walls

// ---- Timing ------------------------------------------------------
#define DEBOUNCE_MS        35
#define LONG_PRESS_MS      550
#define STALE_TIMEOUT_MS  25000UL
#define WIFI_CHANNEL_DWELL_MS   320
#define BLE_SCAN_DURATION_S     3
#define BLE_SCAN_PERIOD_MS      4000UL
#define SWEEP_PERIOD_MS         3200UL   // time for one full radar rotation
#define CAL_SAMPLE_MS           2000UL   // RSSI averaging window during calibration


// ---- Suspicion scoring -------------------------------------------
#define SCORE_FLAG_THRESHOLD   50
#define SCORE_WATCH_THRESHOLD  25
#define STEADY_VARIANCE_DB     4     // <=this dB spread over recent samples = "steady"
#define STEADY_BONUS           8
#define HIDDEN_SSID_BONUS      8
#define RANDOMIZED_MAC_OUI_DIV 3     // discount OUI score by this factor if MAC is private/randomized
#define RANDOMIZED_MAC_SCALE_10 6    // then scale total score by this/10 (0.6x) for randomized MACs

#define MAX_CANDIDATES 24
#define MAX_TRUSTED    16
#define RSSI_HIST_LEN  5

