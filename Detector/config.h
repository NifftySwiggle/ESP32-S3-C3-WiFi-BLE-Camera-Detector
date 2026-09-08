#pragma once
#include <Arduino.h>

// ============================================================
//  Camera / Spy-Cam Detector - pin map & tunables
//  Board: ESP32-C3 or ESP32-S3 (selected automatically by the board target)
// ============================================================

#if defined(CONFIG_IDF_TARGET_ESP32C3)

// ---- Optional External OLED for ESP32-C3 -------------------
// 0 = Original onboard 0.42" 72x40 OLED (Default if no optional screen added)
// 1 = Optional external 0.96" 128x64 I2C OLED (SSD1306)
// 2 = Optional external 0.91" 128x32 I2C OLED (SSD1306)
#define C3_OPTIONAL_OLED 0

#if C3_OPTIONAL_OLED == 1
// ---- Optional 0.96" 128x64 OLED ----------------------------
#define PIN_OLED_SDA   5
#define PIN_OLED_SCL   6
#define OLED_I2C_ADDR  0x3C
#define OLED_W         128
#define OLED_H         64
#define OLED_X_OFFSET  0
#define OLED_Y_OFFSET  0
#define OLED_DRIVER_128X32 0
#define SPLASH_X1 20
#define SPLASH_X2 24
#define SPLASH_Y1 24
#define SPLASH_Y2 40
#define RADAR_CX 96
#define RADAR_CY 32
#define RADAR_MAX_R 28
#define SCREEN_HEADER_Y 10
#define SCREEN_MID_Y 30
#define SCREEN_DBM_Y 46
#define SCREEN_FOOTER_Y 60
#define FINDER_BAR_Y 22
#define FINDER_BAR_W 120
#define FINDER_BAR_H 14
#define PICK_LINE1_Y 28
#define PICK_LINE2_Y 46
#define MENU_BOX_Y 22
#define MENU_BOX_H 20
#define MENU_TEXT_OFFSET  0
#define TEXT_Y0 12
#define TEXT_Y1 28
#define TEXT_Y2 46
#define GAME_TITLE_X 36
#define GAME_TARGET_X 90
#define GAME_TARGET_Y 34
#define GAME_TARGET_R 8

#elif C3_OPTIONAL_OLED == 2
// ---- Optional 0.91" 128x32 OLED ----------------------------
#define PIN_OLED_SDA   5
#define PIN_OLED_SCL   6
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

#else
// ---- Original Onboard 0.42" 72x40 OLED (Default) -----------
#define PIN_OLED_SDA   5
#define PIN_OLED_SCL   6
#define OLED_I2C_ADDR  0x3C
#define OLED_W         72
#define OLED_H         40
#define OLED_X_OFFSET  30
#define OLED_Y_OFFSET  24  // Shift down to align with physical rows 24-63 (removes space under)
#define OLED_DRIVER_128X32 0
#define SPLASH_X1 6
#define SPLASH_X2 10
#define SPLASH_Y1 16
#define SPLASH_Y2 28
#define RADAR_CX 53
#define RADAR_CY 19
#define RADAR_MAX_R 14
#define SCREEN_HEADER_Y 7
#define SCREEN_MID_Y 21
#define SCREEN_DBM_Y 30
#define SCREEN_FOOTER_Y 37
#define FINDER_BAR_Y 11
#define FINDER_BAR_W 64
#define FINDER_BAR_H 7
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
#endif

// ---- 4 Buttons on ESP32-C3 (Ergonomic Gamepad Layout) -------
// Top-Left: Mode / Back (GPIO 3)
// Top-Right: Menu / Select / Action (GPIO 0)
// Bottom-Left: Left / Prev / Sens+ (GPIO 4)
// Bottom-Right: Right / Next / Sens- (GPIO 2)
#define PIN_BTN_MODE   3   // Button 1: Mode / Back (Top-Left)
#define PIN_BTN_SELECT 0   // Button 2: Menu / Select / Action (Top-Right)
#define PIN_BTN_UP     4   // Button 3: Left / Prev / Sens+ (Bottom-Left)
#define PIN_BTN_DOWN   2   // Button 4: Right / Next / Sens- (Bottom-Right)
#define PIN_BUZZER     10  // Buzzer (Active or Passive)
#define PIN_LED        8   // Onboard Blue LED on C3 SuperMini
#define PIN_LED2       1   // Extra external LED on GPIO 1
#define LED_IS_RGB     0
#define USE_4_BUTTONS  1   // 1 = 4-button mode enabled

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

// ---- 4 Buttons on ESP32-S3 ---------------------------------
#define PIN_BTN_MODE   5   // Button 1: Mode / Back / Left
#define PIN_BTN_SELECT 6   // Button 2: Select / Action / Right
#define PIN_BTN_UP     16  // Button 3: Up / Prev / Sens+
#define PIN_BTN_DOWN   47  // Button 4: Down / Next / Sens-
#define PIN_BUZZER     21  // Buzzer (Active or Passive)
#define PIN_LED        48  // Onboard RGB LED (or external)
#define PIN_LED2       -1  // Extra LED disabled by default (-1)
#define LED_IS_RGB     1
#define USE_4_BUTTONS  1   // 1 = 4-button mode enabled on S3
#endif

// ---- Target Filter (Detection Mode) -------------------------------
enum TargetFilter {
  TARGET_CAM_ONLY = 0,   // Camera / surveillance devices only (heuristic score)
  TARGET_ALL_DEV = 1     // Any RF device (WiFi APs, Stations, BLE beacons)
};

// ---- Sensitivity Modes ------------------------------------------
enum SensitivityMode {
  SENS_PINPOINT = 0,  // Ultra close-range sniffing (<0.5m, ~-48dBm threshold)
  SENS_HIGH = 1,      // Close-range (~1.5m, ~-62dBm threshold)
  SENS_MEDIUM = 2,    // Room-wide (~4.0m, ~-74dBm threshold)
  SENS_LOW = 3        // Perimeter / Far (~12.0m, ~-86dBm threshold)
};
#define SENS_COUNT          4

#define SENS_PINPOINT_RSSI  -48
#define SENS_HIGH_RSSI      -62
#define SENS_MEDIUM_RSSI    -74
#define SENS_LOW_RSSI       -86

// ---- Buttons -------------------------------------------------
#define USE_SELECT_BTN  1      // 1 = 2-button or 4-button setup

// ---- Buzzer Configuration (Active & Passive Support) ---------
enum BuzzerType {
  BUZZER_PASSIVE = 0,   // Passive buzzer: pitch frequency modulation via tone()
  BUZZER_ACTIVE  = 1,   // Active buzzer: fixed-frequency on/off via digital HIGH/LOW
  BUZZER_TYPE_COUNT = 2
};
#define DEFAULT_BUZZER_TYPE  BUZZER_PASSIVE
#define BEEP_FREQ_HZ    3000
#define BEEP_ON_MS      70

// ---- RSSI <-> distance model (defaults; overwritten by Calibrate) --
#define TX_REF_RSSI_1M  -40    // typical RSSI at 1m for WiFi/BLE
#define PATH_LOSS_N     2.5f   // 2=free space, 2.5-4=indoor w/ walls

// ---- Timing ------------------------------------------------------
#define DEBOUNCE_MS        35
#define LONG_PRESS_MS      400   // 400ms snappy response
#define STALE_TIMEOUT_MS  25000UL
#define WIFI_CHANNEL_DWELL_MS   320
#define BLE_SCAN_DURATION_S     3
#define BLE_SCAN_PERIOD_MS      4000UL
#define SWEEP_PERIOD_MS         3500UL   // time for one full radar rotation
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

