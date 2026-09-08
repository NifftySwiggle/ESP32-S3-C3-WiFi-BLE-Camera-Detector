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
  u8g2.drawBox(OX(0), OY(0), 3, 2);
  u8g2.drawBox(OX(0), OY(0), 2, 3);
  u8g2.drawBox(OX(OLED_W - 3), OY(OLED_H - 2), 3, 2);
  u8g2.drawBox(OX(OLED_W - 2), OY(OLED_H - 3), 2, 3);

  if (OLED_W < 100) {
    u8g2.setFont(u8g2_font_4x6_tr);
    const char *t1 = "CYBER-DETECT";
    const char *t2 = "RF-SWEEPER v4";
    int w1 = u8g2.getStrWidth(t1);
    int w2 = u8g2.getStrWidth(t2);
    u8g2.drawStr(OX((OLED_W - w1) / 2), OY(13), t1);
    u8g2.drawStr(OX((OLED_W - w2) / 2), OY(22), t2);

    u8g2.drawHLine(OX(6), OY(OLED_H - 6), OLED_W - 12);
    u8g2.drawBox(OX(6), OY(OLED_H - 7), OLED_W - 12, 3);
  } else {
    u8g2.setFont(u8g2_font_5x8_tr);
    const char *t1 = "CYBER-DETECT";
    const char *t2 = "RF-SWEEPER v4";
    int w1 = u8g2.getStrWidth(t1);
    int w2 = u8g2.getStrWidth(t2);
    u8g2.drawStr(OX((OLED_W - w1) / 2), OY(13), t1);
    u8g2.setFont(u8g2_font_4x6_tr);
    u8g2.drawStr(OX((OLED_W - w2) / 2), OY(22), t2);

    u8g2.drawHLine(OX(6), OY(OLED_H - 5), OLED_W - 12);
    u8g2.drawBox(OX(6), OY(OLED_H - 6), OLED_W - 12, 3);
  }
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
  if (OLED_W < 100) {
    // Compact Cockpit HUD for 0.42" (72x40) Display (Radar Scope on Right)
    char hdr[16];
    snprintf(hdr, sizeof(hdr), "R:%s %.3s", fTag, sTag);
    u8g2.drawStr(OX(0), OY(6), hdr);

    char statStr[16];
    if (newDevice) {
      u8g2.drawBox(OX(0), OY(7), 34, 7);
      u8g2.setDrawColor(0);
      u8g2.drawStr(OX(1), OY(13), "NEW!");
      u8g2.setDrawColor(1);
    } else if (targetsInRange > 0) {
      u8g2.drawBox(OX(0), OY(7), 34, 7);
      u8g2.setDrawColor(0);
      snprintf(statStr, sizeof(statStr), "TGT:%d", targetsInRange);
      u8g2.drawStr(OX(1), OY(13), statStr);
      u8g2.setDrawColor(1);
    } else if (allTrusted && totalActive > 0) {
      u8g2.drawStr(OX(0), OY(13), "CLEAR");
    } else if (untrustedActive > 0) {
      snprintf(statStr, sizeof(statStr), "UNT:%d", untrustedActive);
      u8g2.drawStr(OX(0), OY(13), statStr);
    } else {
      u8g2.drawStr(OX(0), OY(13), pulse ? "SCAN." : "SCAN ");
    }

    char devStr[16];
    snprintf(devStr, sizeof(devStr), "DEV:%d", totalActive);
    u8g2.drawStr(OX(0), OY(21), devStr);

#if USE_4_BUTTONS
    if (muted) u8g2.drawStr(OX(0), OY(29), "[MUTE]");
    else u8g2.drawStr(OX(0), OY(29), "0:MENU");
    u8g2.drawStr(OX(0), OY(36), "3:FIND");
#else
    if (muted) u8g2.drawStr(OX(0), OY(29), "[MUTE]");
    else u8g2.drawStr(OX(0), OY(29), "[SENS]");
    u8g2.drawStr(OX(0), OY(36), "3:FIND");
#endif
  } else {
    // Wide HUD for S3 (128x32) or External Displays (128x64)
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
#if USE_4_BUTTONS
    if (muted) snprintf(footStr, sizeof(footStr), "[MUTED] B2:MENU");
    else snprintf(footStr, sizeof(footStr), "[B1]FIND [B2]MENU");
#else
    if (muted) snprintf(footStr, sizeof(footStr), "[MUTED] B2:SENS");
    else snprintf(footStr, sizeof(footStr), "[B1]FIND [B2]SENS");
#endif
    u8g2.drawStr(OX(0), OY(29), footStr);
  }

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

  const char *fPrefix = (targetFilter == TARGET_ALL_DEV) ? "LOCK[ALL]" : "LOCK[CAM]";
  if (!haveTarget) {
    if (OLED_W < 100) {
      u8g2.setFont(u8g2_font_4x6_tr);
      char h[16];
      snprintf(h, sizeof(h), "FIND: %s%s", (targetFilter == TARGET_ALL_DEV) ? "ALL" : "CAM", muted ? " [M]" : "");
      u8g2.drawStr(OX(0), OY(7), h);
      u8g2.drawStr(OX(0), OY(17), "NO TARGET YET");
      u8g2.drawStr(OX(0), OY(27), "SCANNING 2.4GHz");
      u8g2.drawStr(OX(0), OY(36), "3:SURV");
    } else {
      u8g2.setFont(u8g2_font_5x8_tr);
      char h[24]; snprintf(h, sizeof(h), "%s%s", fPrefix, muted ? " [M]" : " SCAN");
      u8g2.drawStr(OX(0), OY(8), h);
      u8g2.setFont(u8g2_font_4x6_tr);
      u8g2.drawStr(OX(0), OY(18), "NO TARGET ACQUIRED");
      u8g2.drawStr(OX(0), OY(27), "SWEEPING FREQUENCIES...");
      u8g2.drawStr(OX(0), OY(35), "[B1] SURVEY");
    }
    u8g2.sendBuffer();
    return;
  }

  // Header with device tag
  const char *tag = c->label[0] ? c->label : (c->hasName ? c->name : "UNKNOWN");
  char hdr[24];
  if (OLED_W < 100) {
    u8g2.setFont(u8g2_font_4x6_tr);
    snprintf(hdr, sizeof(hdr), "F:%.6s%s", tag, c->trusted ? "*" : (unconfirmed ? "?" : ""));
    u8g2.drawStr(OX(0), OY(7), hdr);

    char rightTag[12];
    if (posCount > 0) snprintf(rightTag, sizeof(rightTag), "%s%d/%d", muted ? "M " : "", posIdx + 1, posCount);
    else snprintf(rightTag, sizeof(rightTag), "%s", muted ? "M" : "");
    int rw = u8g2.getStrWidth(rightTag);
    u8g2.drawStr(OX(66 - rw), OY(7), rightTag);
  } else {
    u8g2.setFont(u8g2_font_5x8_tr);
    snprintf(hdr, sizeof(hdr), "F:%.8s%s", tag, c->trusted ? "*" : (unconfirmed ? "?" : ""));
    u8g2.drawStr(OX(0), OY(8), hdr);

    char rightTag[16];
    if (posCount > 0) snprintf(rightTag, sizeof(rightTag), "%s%d/%d", muted ? "M " : "", posIdx + 1, posCount);
    else snprintf(rightTag, sizeof(rightTag), "%s", muted ? "M" : "");
    int rw = u8g2.getStrWidth(rightTag);
    u8g2.drawStr(OX(OLED_W - rw), OY(8), rightTag);
  }

  // Segmented Signal Strength Meter (kept within safe 66px width on 0.42" OLED)
  int barX = (OLED_W < 100) ? 0 : 2;
  int barY = (OLED_W < 100) ? 10 : ((OLED_H >= 40) ? 12 : 10);
  int barW = (OLED_W < 100) ? 66 : FINDER_BAR_W;
  int barH = (OLED_H >= 40) ? 7 : 6;
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
  u8g2.setFont(u8g2_font_4x6_tr);
  if (OLED_W < 100) {
    const char *srcStr = (c->source == SRC_BLE) ? "BLE" : (c->source == SRC_WIFI_AP ? "AP" : "STA");
    snprintf(dbmStr, sizeof(dbmStr), "%ddB %d%% %s", c->rssi, signalPercent, srcStr);
    u8g2.drawStr(OX(0), OY(25), dbmStr);
  } else {
    snprintf(dbmStr, sizeof(dbmStr), "%ddB (%d%%) %s", c->rssi, signalPercent, sourceLabel(c->source));
    int dbmY = (OLED_H >= 40) ? 26 : 23;
    u8g2.drawStr(OX(0), OY(dbmY), dbmStr);
  }

  // Proximity guidance / trend & Sensitivity mode badge
  static int8_t prevRssi = -100;
  const char *trend;
  if (c->trusted) trend = "TRUSTED";
  else if (c->rssi >= -40) trend = (OLED_W < 100) ? "PINPOINT!" : "PINPOINT CONTACT!";
  else if (c->rssi > prevRssi + 1) trend = "WARMER (+)";
  else if (c->rssi < prevRssi - 1) trend = "colder (-)";
  else trend = "STEADY";
  prevRssi = c->rssi;

  char footer[28];
  snprintf(footer, sizeof(footer), "%s [%s]", trend, sensShortLabel(sensMode));
  int footY = (OLED_W < 100) ? 35 : ((OLED_H >= 40) ? 36 : 31);
  u8g2.drawStr(OX(0), OY(footY), footer);

  u8g2.sendBuffer();
}

// Known Devices Picker: High-tech diagnostics card
inline void drawDevicePickScreen(const char *title, const char *footer,
                                  const Candidate *c, int idx, int count) {
  u8g2.clearBuffer();
  u8g2.setFont(u8g2_font_4x6_tr);

  if (OLED_W < 100) {
    const char *shortTitle = (strstr(title, "KNOWN") != NULL) ? "KNOWN DEV" : "SELECT DEV";
    u8g2.drawStr(OX(0), OY(6), shortTitle);
    char pos[10]; snprintf(pos, sizeof(pos), "%d/%d", idx + 1, count);
    int pw = u8g2.getStrWidth(pos);
    u8g2.drawStr(OX(66 - pw), OY(6), pos);
    u8g2.drawHLine(OX(0), OY(8), 66);

    const char *tag = c->label[0] ? c->label : (c->hasName ? c->name : "UNKNOWN");
    char l1[20];
    snprintf(l1, sizeof(l1), "%s%.11s", c->trusted ? "[*] " : "[ ] ", tag);
    u8g2.drawStr(OX(0), OY(16), l1);

    char l2[20];
    const char *srcStr = (c->source == SRC_BLE) ? "BLE" : (c->source == SRC_WIFI_AP ? "WiFi-AP" : "WiFi-STA");
    snprintf(l2, sizeof(l2), "%s %ddBm", srcStr, c->rssi);
    u8g2.drawStr(OX(0), OY(25), l2);

    if (strstr(title, "KNOWN") != NULL) {
      char f[20];
#if USE_4_BUTTONS
      snprintf(f, sizeof(f), "3:BCK 0:%s", c->trusted ? "UNTR" : "TRST");
#else
      snprintf(f, sizeof(f), "0:%s 3:NXT", c->trusted ? "UNTR" : "TRUST");
#endif
      u8g2.drawStr(OX(0), OY(35), f);
    } else {
#if USE_4_BUTTONS
      u8g2.drawStr(OX(0), OY(35), "3:BACK   0:USE");
#else
      u8g2.drawStr(OX(0), OY(35), "3:NEXT   0:USE");
#endif
    }
  } else {
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
  }

  u8g2.sendBuffer();
}

// Futuristic 2-Button System Menu
inline void drawMenuScreen(const char *title, const char *itemLabel, const char *itemValue, int idx, int count) {
  u8g2.clearBuffer();
  u8g2.setFont(u8g2_font_4x6_tr);

  if (OLED_W < 100) {
    u8g2.drawStr(OX(0), OY(6), "SYS MENU");
    char pos[10]; snprintf(pos, sizeof(pos), "%d/%d", idx + 1, count);
    int pw = u8g2.getStrWidth(pos);
    u8g2.drawStr(OX(66 - pw), OY(6), pos);
    u8g2.drawHLine(OX(0), OY(8), 66);

    char itemBuf[24];
    snprintf(itemBuf, sizeof(itemBuf), "> %.14s", itemLabel);
    u8g2.drawStr(OX(0), OY(17), itemBuf);

    char valBuf[24];
    if (itemValue && itemValue[0]) {
      snprintf(valBuf, sizeof(valBuf), "[ %.14s ]", itemValue);
    } else {
      snprintf(valBuf, sizeof(valBuf), "[ SELECT ]");
    }
    int vw = u8g2.getStrWidth(valBuf);
    int vx = (66 - vw) / 2; if (vx < 0) vx = 0;
    u8g2.drawStr(OX(vx), OY(26), valBuf);

#if USE_4_BUTTONS
    u8g2.drawStr(OX(0), OY(36), "3:BACK   0:OK");
#else
    u8g2.drawStr(OX(0), OY(36), "3:NEXT   0:OK");
#endif
  } else {
    char hdr[24];
    snprintf(hdr, sizeof(hdr), "/// %s ///  %02d/%02d", title, idx + 1, count);
    u8g2.drawStr(OX(0), OY(6), hdr);
    u8g2.drawHLine(OX(0), OY(8), OLED_W);

    u8g2.setFont(u8g2_font_5x8_tr);
    char itemBuf[24];
    snprintf(itemBuf, sizeof(itemBuf), "> %s", itemLabel);
    u8g2.drawStr(OX(0), OY(17), itemBuf);

    u8g2.setFont(u8g2_font_4x6_tr);
    char valBuf[28];
    if (itemValue && itemValue[0]) {
      snprintf(valBuf, sizeof(valBuf), "[ %s ]", itemValue);
    } else {
      snprintf(valBuf, sizeof(valBuf), "[ SELECT ]");
    }
    u8g2.drawStr(OX(6), OY(25), valBuf);

#if USE_4_BUTTONS
    u8g2.drawStr(OX(0), OY(31), "[B1] BACK   [B2] CHANGE");
#else
    u8g2.drawStr(OX(0), OY(31), "[B1] NEXT   [B2] CHANGE");
#endif
  }

  u8g2.sendBuffer();
}

// Generic instruction screen for calibration wizard
inline void drawTextScreen(const char *l0, const char *l1, const char *l2, const char *l3) {
  u8g2.clearBuffer();
  if (OLED_W < 100) {
    u8g2.setFont(u8g2_font_4x6_tr);
    if (l0 && l0[0]) u8g2.drawStr(OX(0), OY(7), l0);
    if (l1 && l1[0]) u8g2.drawStr(OX(0), OY(16), l1);
    if (l2 && l2[0]) u8g2.drawStr(OX(0), OY(25), l2);
    if (l3 && l3[0]) u8g2.drawStr(OX(0), OY(35), l3);
  } else {
    u8g2.setFont(u8g2_font_5x8_tr);
    if (l0 && l0[0]) u8g2.drawStr(OX(0), OY(8), l0);
    u8g2.setFont(u8g2_font_4x6_tr);
    if (l1 && l1[0]) u8g2.drawStr(OX(0), OY(16), l1);
    if (l2 && l2[0]) u8g2.drawStr(OX(0), OY(24), l2);
    if (l3 && l3[0]) u8g2.drawStr(OX(0), OY(31), l3);
  }
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

  if (finished) {
    if (OLED_W < 100) {
      u8g2.setFont(u8g2_font_4x6_tr);
      if (newHighScore) {
        u8g2.drawStr(OX(0), OY(7), "* NEW RECORD! *");
        char sc[20]; snprintf(sc, sizeof(sc), "SCORE: %03d", score);
        u8g2.drawStr(OX(0), OY(16), sc);
      } else {
        char hdr[20]; snprintf(hdr, sizeof(hdr), "CRASH! SC:%03d", score);
        u8g2.drawStr(OX(0), OY(7), hdr);
        char hs[20]; snprintf(hs, sizeof(hs), "BEST: %03d", highScore);
        u8g2.drawStr(OX(0), OY(16), hs);
      }
#if USE_4_BUTTONS
      u8g2.drawStr(OX(0), OY(26), "B2: REPLAY");
      u8g2.drawStr(OX(0), OY(36), "B1: EXIT");
#else
      u8g2.drawStr(OX(0), OY(26), "B1/B2: REPLAY");
      u8g2.drawStr(OX(0), OY(36), "HOLD B1: EXIT");
#endif
    } else {
      u8g2.setFont(u8g2_font_5x8_tr);
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
#if USE_4_BUTTONS
      u8g2.drawStr(OX(0), OY(25), "[B2] PLAY AGAIN");
      u8g2.drawStr(OX(0), OY(31), "[B1] EXIT");
#else
      u8g2.drawStr(OX(0), OY(25), "[B1/B2] PLAY AGAIN");
      u8g2.drawStr(OX(0), OY(31), "HOLD [B1] EXIT");
#endif
    }
    u8g2.sendBuffer();
    return;
  }

  if (!started) {
    if (OLED_W < 100) {
      u8g2.setFont(u8g2_font_4x6_tr);
      u8g2.drawStr(OX(0), OY(7), "// SPY EVADER //");
      char hs[20]; snprintf(hs, sizeof(hs), "RECORD: %03d", highScore);
      u8g2.drawStr(OX(0), OY(16), hs);
      u8g2.drawStr(OX(0), OY(26), "B1:LEFT  B2:RIGHT");
#if USE_4_BUTTONS
      u8g2.drawStr(OX(0), OY(36), "B2:START B1:EXIT");
#else
      u8g2.drawStr(OX(0), OY(36), "PRESS B1/B2 GO");
#endif
    } else {
      u8g2.setFont(u8g2_font_5x8_tr);
      u8g2.drawStr(OX(0), OY(8), "/// SPY EVADER ///");
      char hs[24]; snprintf(hs, sizeof(hs), "RECORD: %03d PTS", highScore);
      u8g2.setFont(u8g2_font_4x6_tr);
      u8g2.drawStr(OX(0), OY(17), hs);
      u8g2.drawStr(OX(0), OY(25), "B1:LEFT   B2:RIGHT");
#if USE_4_BUTTONS
      u8g2.drawStr(OX(0), OY(31), "[B2] START  [B1] EXIT");
#else
      u8g2.drawStr(OX(0), OY(31), "PRESS B1/B2 START");
#endif
    }
    u8g2.sendBuffer();
    return;
  }

  // Active Game HUD
  u8g2.setFont(u8g2_font_4x6_tr);
  char header[16];
  snprintf(header, sizeof(header), "SC:%d", score);
  u8g2.drawStr(OX(0), OY(6), header);

  char hiStr[16];
  snprintf(hiStr, sizeof(hiStr), "HI:%d", highScore);
  int hiW = u8g2.getStrWidth(hiStr);
  u8g2.drawStr(OX(OLED_W - hiW), OY(6), hiStr);

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

// ============================================================
//  Wi-Fi Signal & Range Survey Mode Screen
//  Real-time RSSI meter, sweet spot evaluator, and live sparkline
// ============================================================
inline void drawSurveyScreen(const Candidate *c, int apIdx, int apCount,
                             int8_t peakRssi, int8_t minRssi,
                             const int8_t *history, int histCount,
                             bool muted) {
  u8g2.clearBuffer();
  u8g2.setFont(u8g2_font_4x6_tr);

  if (!c || !c->active) {
    if (OLED_W < 100) {
      u8g2.drawStr(OX(0), OY(6), "WIFI SURVEY");
      u8g2.drawHLine(OX(0), OY(8), 66);
      u8g2.drawStr(OX(0), OY(18), "SCANNING 2.4G...");
      u8g2.drawStr(OX(0), OY(27), "SEARCHING APs");
      u8g2.drawStr(OX(0), OY(36), "[B1] RADAR");
    } else {
      u8g2.setFont(u8g2_font_5x8_tr);
      u8g2.drawStr(OX(0), OY(8), "/// WIFI SURVEY MODE ///");
      u8g2.drawHLine(OX(0), OY(10), OLED_W);
      u8g2.setFont(u8g2_font_4x6_tr);
      u8g2.drawStr(OX(0), OY(19), "SCANNING 2.4GHz CHANNELS 1-13...");
      u8g2.drawStr(OX(0), OY(27), "SEARCHING FOR WIRELESS ROUTERS & APs");
      u8g2.drawStr(OX(0), OY(35), "[B1] RADAR");
    }
    u8g2.sendBuffer();
    return;
  }

  // Identify network & ratings
  const char *name = (c->hasName && c->name[0]) ? c->name : (c->label[0] ? c->label : "WIFI AP");
  int qual = constrain(map(c->rssi, -90, -30, 0, 100), 0, 100);

  const char *rating;
  const char *advice;
  if (c->rssi >= -50) {
    rating = (OLED_W < 100) ? "BEST" : "BEST SPOT";
    advice = "MAX SPEED / 4K";
  } else if (c->rssi >= -62) {
    rating = "GREAT";
    advice = "IDEAL EXTENDER";
  } else if (c->rssi >= -72) {
    rating = "GOOD";
    advice = "NORMAL RANGE";
  } else if (c->rssi >= -80) {
    rating = "WEAK";
    advice = "MOVE CLOSER";
  } else {
    rating = (OLED_W < 100) ? "DEAD" : "DEAD ZONE";
    advice = "RELOCATE AP";
  }

  if (OLED_W < 100) {
    // 0.42" 72x40 Compact Cockpit Layout (Max safe width: 66px to fit glass bezel)
    // Header Line (Y=6)
    char hdr[16];
    snprintf(hdr, sizeof(hdr), "%.5s C%d", name, c->channel);
    u8g2.drawStr(OX(0), OY(6), hdr);

    char pos[10];
    if (apCount > 0) snprintf(pos, sizeof(pos), "%d/%d", apIdx + 1, apCount);
    else snprintf(pos, sizeof(pos), "%s", muted ? "M" : "");
    int pw = u8g2.getStrWidth(pos);
    u8g2.drawStr(OX(66 - pw), OY(6), pos);
    u8g2.drawHLine(OX(0), OY(8), 66);

    // Live Readout & Placement Rating (Y=15)
    char stat[20];
    snprintf(stat, sizeof(stat), "%ddB %s %d%%", c->rssi, rating, qual);
    u8g2.drawStr(OX(0), OY(15), stat);

    // Left: Segmented Signal Bar with Peak Hold cursor (Y=18, W=30, H=8)
    int barX = 0, barY = 18, barW = 30, barH = 8;
    u8g2.drawFrame(OX(barX), OY(barY), barW, barH);
    int fillW = map(constrain(c->rssi, -90, -30), -90, -30, 0, barW - 2);
    if (fillW > 0) u8g2.drawBox(OX(barX + 1), OY(barY + 1), fillW, barH - 2);
    if (peakRssi >= -90) {
      int pkX = map(constrain(peakRssi, -90, -30), -90, -30, 0, barW - 2);
      if (pkX > 0 && pkX < barW - 1) {
        u8g2.drawVLine(OX(barX + 1 + pkX), OY(barY - 1), barH + 2);
      }
    }

    // Right: Live Rolling History Sparkline (Y=18, W=33, H=8, ends at X=66)
    int gx = 33, gy = 18, gw = 33, gh = 8;
    u8g2.drawFrame(OX(gx), OY(gy), gw, gh);
    if (histCount > 1) {
      int prevPx = -1, prevPy = -1;
      int maxSamples = (gw - 2) / 2;
      int startIdx = (histCount > maxSamples) ? (histCount - maxSamples) : 0;
      int countToDraw = histCount - startIdx;
      for (int i = 0; i < countToDraw; i++) {
        int sIdx = startIdx + i;
        int px = gx + 1 + (i * 2);
        int py = gy + gh - 2 - map(constrain(history[sIdx], -90, -30), -90, -30, 0, gh - 3);
        if (prevPx >= 0) {
          u8g2.drawLine(OX(prevPx), OY(prevPy), OX(px), OY(py));
        } else {
          u8g2.drawPixel(OX(px), OY(py));
        }
        prevPx = px;
        prevPy = py;
      }
    }

    // Telemetry Footer (Y=36)
    char foot[16];
    snprintf(foot, sizeof(foot), "PK:%ddB", peakRssi);
    u8g2.drawStr(OX(0), OY(36), foot);
    const char *prompt = "3:RAD";
    int bw = u8g2.getStrWidth(prompt);
    u8g2.drawStr(OX(66 - bw), OY(36), prompt);
  } else {
    // 128-pixel Wide Layout (128x32 or 128x64)
    u8g2.setFont(u8g2_font_5x8_tr);
    char hdr[32];
    snprintf(hdr, sizeof(hdr), "SURVEY: %.10s C:%d", name, c->channel);
    u8g2.drawStr(OX(0), OY(8), hdr);

    char pos[16];
    if (apCount > 0) snprintf(pos, sizeof(pos), "%s%d/%d", muted ? "M " : "", apIdx + 1, apCount);
    else snprintf(pos, sizeof(pos), "%s", muted ? "[M]" : "");
    int pw = u8g2.getStrWidth(pos);
    u8g2.drawStr(OX(OLED_W - pw), OY(8), pos);

    u8g2.setFont(u8g2_font_4x6_tr);
    char stat[40];
    snprintf(stat, sizeof(stat), "%ddBm [%d%%]  %s (%s)", c->rssi, qual, rating, advice);
    int statY = (OLED_H >= 40) ? 18 : 17;
    u8g2.drawStr(OX(0), OY(statY), stat);

    // Left: Wide Segmented Signal Bar
    int barX = 0;
    int barY = (OLED_H >= 40) ? 22 : 19;
    int barW = (OLED_W >= 120) ? 68 : 50;
    int barH = (OLED_H >= 40) ? 9 : 6;
    u8g2.drawFrame(OX(barX), OY(barY), barW, barH);
    int fillW = map(constrain(c->rssi, -90, -30), -90, -30, 0, barW - 2);
    if (fillW > 0) u8g2.drawBox(OX(barX + 1), OY(barY + 1), fillW, barH - 2);
    if (peakRssi >= -90) {
      int pkX = map(constrain(peakRssi, -90, -30), -90, -30, 0, barW - 2);
      if (pkX > 0 && pkX < barW - 1) {
        u8g2.drawVLine(OX(barX + 1 + pkX), OY(barY - 1), barH + 2);
      }
    }

    // Right: Live History Sparkline Graph
    int gx = barX + barW + 4;
    int gw = OLED_W - gx - 1;
    int gy = barY;
    int gh = barH;
    u8g2.drawFrame(OX(gx), OY(gy), gw, gh);
    if (histCount > 1) {
      int prevPx = -1, prevPy = -1;
      int maxSamples = (gw - 2) / 2;
      int startIdx = (histCount > maxSamples) ? (histCount - maxSamples) : 0;
      int countToDraw = histCount - startIdx;
      for (int i = 0; i < countToDraw; i++) {
        int sIdx = startIdx + i;
        int px = gx + 1 + (i * 2);
        int py = gy + gh - 2 - map(constrain(history[sIdx], -90, -30), -90, -30, 0, gh - 3);
        if (prevPx >= 0) {
          u8g2.drawLine(OX(prevPx), OY(prevPy), OX(px), OY(py));
        } else {
          u8g2.drawPixel(OX(px), OY(py));
        }
        prevPx = px;
        prevPy = py;
      }
    }

    // Footer
    char foot[40];
    snprintf(foot, sizeof(foot), "PK:%ddB MN:%ddB  [B1]RAD [B2]RST [B3/4]AP", peakRssi, minRssi);
    int footY = (OLED_H >= 40) ? 37 : 31;
    u8g2.drawStr(OX(0), OY(footY), foot);
  }

  u8g2.sendBuffer();
}


