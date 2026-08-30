#pragma once
#include <Wire.h>
#include <U8g2lib.h>
#include <math.h>
#include "config.h"
#include "candidate_store.h"

#if OLED_DRIVER_128X32
U8G2_SSD1306_128X32_UNIVISION_F_HW_I2C u8g2(U8G2_R0, U8X8_PIN_NONE, PIN_OLED_SCL, PIN_OLED_SDA);
#else
U8G2_SSD1306_128X64_NONAME_F_HW_I2C u8g2(U8G2_R0, U8X8_PIN_NONE, PIN_OLED_SCL, PIN_OLED_SDA);
#endif

inline int OX(int x) { return x + OLED_X_OFFSET; }
inline int OY(int y) { return y + OLED_Y_OFFSET; }

inline void displayInit() {
  Wire.begin(PIN_OLED_SDA, PIN_OLED_SCL);
  Serial.printf("I2C scan SDA=%d SCL=%d: ", PIN_OLED_SDA, PIN_OLED_SCL);
  int found = 0;
  for (uint8_t address = 1; address < 127; address++) {
    Wire.beginTransmission(address);
    if (Wire.endTransmission() == 0) {
      Serial.printf("0x%02X ", address);
      found++;
    }
  }
  Serial.printf("(%d device%s)\n", found, found == 1 ? "" : "s");
  u8g2.setI2CAddress(OLED_I2C_ADDR << 1);
  u8g2.begin();
  u8g2.setContrast(255);
  u8g2.setBusClock(400000);
}

inline void displaySplash() {
  u8g2.clearBuffer();
  // Sci-fi tech border frame
  u8g2.drawFrame(OX(0), OY(0), OLED_W, OLED_H);
  u8g2.drawBox(OX(0), OY(0), 4, 2);
  u8g2.drawBox(OX(0), OY(0), 2, 4);
  u8g2.drawBox(OX(OLED_W - 4), OY(OLED_H - 2), 4, 2);
  u8g2.drawBox(OX(OLED_W - 2), OY(OLED_H - 4), 2, 4);

  u8g2.setFont(u8g2_font_5x8_tr);
  const char *t1 = "CYBER-DETECT";
  const char *t2 = "RF-SWEEPER v4";
  int w1 = u8g2.getStrWidth(t1);
  int w2 = u8g2.getStrWidth(t2);
  u8g2.drawStr(OX((OLED_W - w1) / 2), OY(13), t1);
  u8g2.setFont(u8g2_font_4x6_tr);
  u8g2.drawStr(OX((OLED_W - w2) / 2), OY(22), t2);

  // Animated futuristic scanline bar
  u8g2.drawHLine(OX(6), OY(OLED_H - 5), OLED_W - 12);
  u8g2.drawBox(OX(6), OY(OLED_H - 6), OLED_W - 12, 3);
  u8g2.sendBuffer();
  delay(1000);
}

// stable pseudo-angle per MAC so a device stays in the same radar slot
inline float macToAngle(const uint8_t mac[6]) {
  uint16_t h = 0;
  for (int i = 0; i < 6; i++) h = (h * 131) + mac[i];
  return (h % 360) * (PI / 180.0f);
}

inline int rssiToRadiusPx(int8_t rssi, int maxR) {
  return map(constrain(rssi, -95, -30), -95, -30, maxR, 3);
}

inline const char* sensShortLabel(SensitivityMode m) {
  switch (m) {
    case SENS_PINPOINT: return "PIN";
    case SENS_HIGH:     return "HI";
    case SENS_MEDIUM:   return "MED";
    case SENS_LOW:      return "LOW";
    default:            return "STD";
  }
}

inline void drawRadarScreen(float sweepAngle, float rangeM, int targetsInRange,
                             int totalActive, int untrustedActive,
                             bool allTrusted, bool muted, bool pulse,
                             bool newDevice, TargetFilter targetFilter,
                             SensitivityMode sensMode,
                             float pingContactAngle = -1.0f) {
  u8g2.clearBuffer();
  u8g2.setFont(u8g2_font_4x6_tr);

  const int cx = RADAR_CX, cy = RADAR_CY, maxR = RADAR_MAX_R;
  const char *fTag = (targetFilter == TARGET_ALL_DEV) ? "ALL" : "CAM";
  const char *sTag = sensShortLabel(sensMode);

  // --- Left / Top HUD Telemetry ---
  char hdr[20];
  snprintf(hdr, sizeof(hdr), "RAD[%s] %s", fTag, sTag);
  u8g2.drawStr(OX(0), OY(6), hdr);

  char statStr[20];
  if (newDevice) {
    snprintf(statStr, sizeof(statStr), "! NEW CONTACT");
    u8g2.drawBox(OX(0), OY(8), 54, 8);
    u8g2.setDrawColor(0);
    u8g2.drawStr(OX(2), OY(14), statStr);
    u8g2.setDrawColor(1);
  } else if (targetsInRange > 0) {
    snprintf(statStr, sizeof(statStr), "! TARGETS: %02d", targetsInRange);
    u8g2.drawBox(OX(0), OY(8), 56, 8);
    u8g2.setDrawColor(0);
    u8g2.drawStr(OX(2), OY(14), statStr);
    u8g2.setDrawColor(1);
  } else if (allTrusted && totalActive > 0) {
    u8g2.drawStr(OX(0), OY(14), "STATUS: CLEAR");
  } else if (untrustedActive > 0) {
    snprintf(statStr, sizeof(statStr), "UNTRUST: %02d", untrustedActive);
    u8g2.drawStr(OX(0), OY(14), statStr);
  } else {
    snprintf(statStr, sizeof(statStr), pulse ? "SWEEPING ." : "SWEEPING...");
    u8g2.drawStr(OX(0), OY(14), statStr);
  }

  char devStr[20];
  snprintf(devStr, sizeof(devStr), "DEVICES: %02d", totalActive);
  u8g2.drawStr(OX(0), OY(21), devStr);

  char footStr[20];
  if (muted) snprintf(footStr, sizeof(footStr), "[MUTED] B2:SENS");
  else snprintf(footStr, sizeof(footStr), "[B1]FIND [B2]SENS");
  u8g2.drawStr(OX(0), OY(29), footStr);

  // --- Sci-Fi Radar Scope Graphics ---
  // Outer range ring and mid ring
  u8g2.drawCircle(OX(cx), OY(cy), maxR);
  u8g2.drawCircle(OX(cx), OY(cy), maxR / 2);
  // Center reticle
  u8g2.drawPixel(OX(cx), OY(cy));

  // Cardinal tick marks at N, S, E, W
  u8g2.drawPixel(OX(cx), OY(cy - maxR - 1));
  u8g2.drawPixel(OX(cx), OY(cy + maxR + 1));
  u8g2.drawPixel(OX(cx - maxR - 1), OY(cy));
  u8g2.drawPixel(OX(cx + maxR + 1), OY(cy));

  // Single crisp sweeping ray line
  int sx = cx + (int)(cosf(sweepAngle) * maxR);
  int sy = cy + (int)(sinf(sweepAngle) * maxR);
  u8g2.drawLine(OX(cx), OY(cy), OX(sx), OY(sy));

  // Device contacts on radar
  for (int i = 0; i < MAX_CANDIDATES; i++) {
    if (!g_candidates[i].active) continue;
    float ang = macToAngle(g_candidates[i].mac);
    int r = rssiToRadiusPx(g_candidates[i].rssi, maxR - 1);
    int bx = cx + (int)(cosf(ang) * r);
    int by = cy + (int)(sinf(ang) * r);
    bool pingContact = pulse && pingContactAngle >= 0.0f && fabsf(ang - pingContactAngle) < 0.001f;

    if (g_candidates[i].trusted) {
      u8g2.drawPixel(OX(bx), OY(by));   // 1px micro-dot for trusted baseline
    } else if (targetFilter == TARGET_ALL_DEV) {
      if (pingContact) {
        // Swept-over target: 2px tactical lock ring + center dot
        u8g2.drawCircle(OX(bx), OY(by), 2);
        u8g2.drawPixel(OX(bx), OY(by));
      } else {
        // Sharp small 2x2 target dot
        u8g2.drawBox(OX(bx), OY(by), 2, 2);
      }
    } else {
      // Camera mode
      if (g_candidates[i].score >= SCORE_FLAG_THRESHOLD) {
        if (pingContact) {
          u8g2.drawCircle(OX(bx), OY(by), 2);
          u8g2.drawPixel(OX(bx), OY(by));
        } else {
          u8g2.drawBox(OX(bx), OY(by), 2, 2);
        }
      } else if (g_candidates[i].score >= SCORE_WATCH_THRESHOLD) {
        u8g2.drawPixel(OX(bx), OY(by));
        u8g2.drawPixel(OX(bx + 1), OY(by));
      } else {
        u8g2.drawPixel(OX(bx), OY(by));
      }
    }
  }

  u8g2.sendBuffer();
}

inline const char* sourceLabel(uint8_t src) {
  switch (src) {
    case SRC_WIFI_AP:  return "WiFi-AP";
    case SRC_WIFI_STA: return "WiFi-STA";
    case SRC_BLE:       return "BLE-ADV";
  }
  return "RF-DEV";
}

inline void drawFinderScreen(bool haveTarget, const Candidate *c, bool unconfirmed, bool muted,
                              int posIdx, int posCount, TargetFilter targetFilter,
                              SensitivityMode sensMode, int8_t peakRssi) {
  u8g2.clearBuffer();
  u8g2.setFont(u8g2_font_5x8_tr);

  const char *fPrefix = (targetFilter == TARGET_ALL_DEV) ? "LOCK[ALL]" : "LOCK[CAM]";
  if (!haveTarget) {
    char h[24]; snprintf(h, sizeof(h), "%s%s", fPrefix, muted ? " [M]" : " SCAN");
    u8g2.drawStr(OX(0), OY(8), h);
    u8g2.setFont(u8g2_font_4x6_tr);
    u8g2.drawStr(OX(0), OY(18), "NO TARGET ACQUIRED");
    u8g2.drawStr(OX(0), OY(27), "SWEEPING FREQUENCIES...");
    u8g2.drawStr(OX(0), OY(35), "[B1] RADAR");
    u8g2.sendBuffer();
    return;
  }

  // Header with device tag
  const char *tag = c->label[0] ? c->label : (c->hasName ? c->name : "UNKNOWN");
  char hdr[24];
  snprintf(hdr, sizeof(hdr), "F:%.6s%s", tag, c->trusted ? "*" : (unconfirmed ? "?" : ""));
  u8g2.drawStr(OX(0), OY(8), hdr);

  char rightTag[16];
  if (posCount > 0) snprintf(rightTag, sizeof(rightTag), "%s%d/%d", muted ? "M " : "", posIdx + 1, posCount);
  else snprintf(rightTag, sizeof(rightTag), "%s", muted ? "M" : "");
  int rw = u8g2.getStrWidth(rightTag);
  u8g2.drawStr(OX(OLED_W - rw), OY(8), rightTag);

  // Segmented Signal Strength Meter
  int barX = 2, barY = 10, barW = FINDER_BAR_W, barH = 6;
  u8g2.drawFrame(OX(barX), OY(barY), barW, barH);
  int fillPx = map(constrain(c->rssi, -95, -30), -95, -30, 0, barW - 2);
  if (fillPx > 0) u8g2.drawBox(OX(barX + 1), OY(barY + 1), fillPx, barH - 2);

  // Peak signal cursor marker
  if (peakRssi >= -95) {
    int peakPx = map(constrain(peakRssi, -95, -30), -95, -30, 0, barW - 2);
    if (peakPx > 0 && peakPx < barW - 1) {
      u8g2.drawVLine(OX(barX + 1 + peakPx), OY(barY - 1), barH + 2);
    }
  }

  // Telemetry Line: RSSI dBm + Proximity % + Source Type
  int signalPercent = map(constrain(c->rssi, -95, -30), -95, -30, 0, 100);
  char dbmStr[24];
  snprintf(dbmStr, sizeof(dbmStr), "%ddB (%d%%) %s", c->rssi, signalPercent, sourceLabel(c->source));
  u8g2.setFont(u8g2_font_4x6_tr);
  u8g2.drawStr(OX(0), OY(23), dbmStr);

  // Proximity guidance / trend & Sensitivity mode badge
  static int8_t prevRssi = -100;
  const char *trend;
  if (c->trusted) trend = "TRUSTED BASE";
  else if (c->rssi >= -40) trend = "PINPOINT CONTACT!";
  else if (c->rssi > prevRssi + 1) trend = "WARMER (+)";
  else if (c->rssi < prevRssi - 1) trend = "colder (-)";
  else trend = "STEADY";
  prevRssi = c->rssi;

  char footer[28];
  snprintf(footer, sizeof(footer), "%s [%s]", trend, sensShortLabel(sensMode));
  u8g2.drawStr(OX(0), OY(31), footer);

  u8g2.sendBuffer();
}

// Known Devices Picker: High-tech diagnostics card
inline void drawDevicePickScreen(const char *title, const char *footer,
                                  const Candidate *c, int idx, int count) {
  u8g2.clearBuffer();
  u8g2.setFont(u8g2_font_4x6_tr);

  char hdr[24];
  snprintf(hdr, sizeof(hdr), "/// %s /// %d/%d", title, idx + 1, count);
  u8g2.drawStr(OX(0), OY(6), hdr);
  u8g2.drawHLine(OX(0), OY(8), OLED_W);

  u8g2.setFont(u8g2_font_5x8_tr);
  const char *tag = c->label[0] ? c->label : (c->hasName ? c->name : "UNKNOWN");
  char l1[24];
  snprintf(l1, sizeof(l1), "%s%.10s", c->trusted ? "[*] " : "[ ] ", tag);
  u8g2.drawStr(OX(0), OY(18), l1);

  char l2[24];
  snprintf(l2, sizeof(l2), "%s  %ddBm", sourceLabel(c->source), c->rssi);
  u8g2.setFont(u8g2_font_4x6_tr);
  u8g2.drawStr(OX(0), OY(25), l2);

  u8g2.drawStr(OX(0), OY(31), footer);

  u8g2.sendBuffer();
}

// Futuristic 2-Button System Menu
inline void drawMenuScreen(const char *title, const char *itemLabel, const char *itemValue, int idx, int count) {
  u8g2.clearBuffer();
  u8g2.setFont(u8g2_font_4x6_tr);

  // Top header bar with index
  char hdr[24];
  if (OLED_W >= 100) {
    snprintf(hdr, sizeof(hdr), "/// %s ///  %02d/%02d", title, idx + 1, count);
  } else {
    snprintf(hdr, sizeof(hdr), "// %s // %d/%d", title, idx + 1, count);
  }
  u8g2.drawStr(OX(0), OY(6), hdr);
  u8g2.drawHLine(OX(0), OY(8), OLED_W);

  // Highlighted item title
  u8g2.setFont(u8g2_font_5x8_tr);
  char itemBuf[24];
  snprintf(itemBuf, sizeof(itemBuf), "> %s", itemLabel);
  u8g2.drawStr(OX(0), OY(17), itemBuf);

  // Value badge in sci-fi bracket
  u8g2.setFont(u8g2_font_4x6_tr);
  char valBuf[28];
  if (itemValue && itemValue[0]) {
    snprintf(valBuf, sizeof(valBuf), "[ %s ]", itemValue);
  } else {
    snprintf(valBuf, sizeof(valBuf), "[ SELECT ]");
  }
  u8g2.drawStr(OX(6), OY(25), valBuf);

  // Footer: 2-Button controls guide
  if (OLED_W >= 100) {
    u8g2.drawStr(OX(0), OY(31), "[B1] NEXT   [B2] CHANGE");
  } else {
    u8g2.drawStr(OX(0), OY(38), "B1:NEXT  B2:OK");
  }

  u8g2.sendBuffer();
}

// Generic instruction screen for calibration wizard
inline void drawTextScreen(const char *l0, const char *l1, const char *l2, const char *l3) {
  u8g2.clearBuffer();
  u8g2.setFont(u8g2_font_5x8_tr);
  if (l0 && l0[0]) u8g2.drawStr(OX(0), OY(8), l0);
  u8g2.setFont(u8g2_font_4x6_tr);
  if (l1 && l1[0]) u8g2.drawStr(OX(0), OY(16), l1);
  if (l2 && l2[0]) u8g2.drawStr(OX(0), OY(24), l2);
  if (l3 && l3[0]) u8g2.drawStr(OX(0), OY(31), l3);
  u8g2.sendBuffer();
}

struct GameEntity {
  int8_t  lane;    // 0, 1, 2
  int8_t  y;       // position down screen
  uint8_t type;    // 0 = Obstacle drone, 1 = Collectible bonus data packet (+5 pts)
  bool    active;
};

// Futuristic Cyberpunk Arcade Minigame: SPY EVADER
inline void drawGameScreen(int lane, const GameEntity *entities, int entityCount,
                           int score, int highScore, bool started, bool finished,
                           bool newHighScore, uint8_t roadAnimOffset) {
  u8g2.clearBuffer();
  u8g2.setFont(u8g2_font_5x8_tr);

  if (finished) {
    if (newHighScore) {
      u8g2.drawStr(OX(0), OY(8), "* NEW HIGH SCORE! *");
      char sc[24]; snprintf(sc, sizeof(sc), "SCORE: %03d", score);
      u8g2.drawStr(OX(0), OY(17), sc);
    } else {
      char hdr[24]; snprintf(hdr, sizeof(hdr), "CRASHED! SC:%03d", score);
      u8g2.drawStr(OX(0), OY(8), hdr);
      char hs[24]; snprintf(hs, sizeof(hs), "BEST: %03d", highScore);
      u8g2.drawStr(OX(0), OY(17), hs);
    }
    u8g2.setFont(u8g2_font_4x6_tr);
    u8g2.drawStr(OX(0), OY(25), "[B1/B2] PLAY AGAIN");
    u8g2.drawStr(OX(0), OY(31), "HOLD [B1] EXIT");
    u8g2.sendBuffer();
    return;
  }

  if (!started) {
    u8g2.drawStr(OX(0), OY(8), "/// SPY EVADER ///");
    char hs[24]; snprintf(hs, sizeof(hs), "RECORD: %03d PTS", highScore);
    u8g2.setFont(u8g2_font_4x6_tr);
    u8g2.drawStr(OX(0), OY(17), hs);
    u8g2.drawStr(OX(0), OY(25), "B1:LEFT   B2:RIGHT");
    u8g2.drawStr(OX(0), OY(31), "PRESS B1/B2 START");
    u8g2.sendBuffer();
    return;
  }

  // Active Game HUD
  char header[24];
  snprintf(header, sizeof(header), "SC:%03d", score);
  u8g2.drawStr(OX(0), OY(7), header);

  char hiStr[16];
  snprintf(hiStr, sizeof(hiStr), "HI:%03d", highScore);
  int hiW = u8g2.getStrWidth(hiStr);
  u8g2.drawStr(OX(OLED_W - hiW), OY(7), hiStr);

  int laneW = OLED_W / 3;
  int laneX[3] = { laneW / 2, laneW + laneW / 2, laneW * 2 + laneW / 2 };

  // Road dividers with dashed scrolling animation
  int div1 = laneW;
  int div2 = laneW * 2;
  for (int y = 8; y < OLED_H; y += 6) {
    int dy = y + (roadAnimOffset % 6);
    if (dy >= 8 && dy < OLED_H) {
      u8g2.drawPixel(OX(div1), OY(dy));
      u8g2.drawPixel(OX(div2), OY(dy));
    }
  }

  // Draw obstacles & collectibles
  for (int i = 0; i < entityCount; i++) {
    if (!entities[i].active) continue;
    int cx = laneX[entities[i].lane];
    int cy = entities[i].y;
    if (cy >= 4 && cy <= OLED_H + 4) {
      if (entities[i].type == 0) {
        // Enemy Surveillance Drone / Obstacle
        u8g2.drawBox(OX(cx - 3), OY(cy), 7, 4);
        u8g2.drawPixel(OX(cx), OY(cy + 1));
      } else {
        // Bonus Data Packet (+)
        u8g2.drawPixel(OX(cx), OY(cy - 2));
        u8g2.drawPixel(OX(cx - 2), OY(cy));
        u8g2.drawPixel(OX(cx + 2), OY(cy));
        u8g2.drawPixel(OX(cx), OY(cy + 2));
        u8g2.drawPixel(OX(cx), OY(cy));
      }
    }
  }

  // Draw Player Cyber-Vehicle with thruster flames
  int playerY = OLED_H - 7;
  int px = laneX[lane];
  u8g2.drawBox(OX(px - 4), OY(playerY), 9, 5);
  u8g2.drawPixel(OX(px), OY(playerY + 1));
  u8g2.drawPixel(OX(px - 4), OY(playerY + 5));
  u8g2.drawPixel(OX(px + 4), OY(playerY + 5));

  u8g2.sendBuffer();
}


