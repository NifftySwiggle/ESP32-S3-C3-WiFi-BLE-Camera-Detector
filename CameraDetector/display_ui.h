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
  u8g2.setFont(u8g2_font_4x6_tr);
  u8g2.drawStr(OX(SPLASH_X1), OY(SPLASH_Y1), "CAMERA");
  u8g2.drawStr(OX(SPLASH_X2), OY(SPLASH_Y2), "FINDER");
  u8g2.sendBuffer();
  delay(1200);
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

// Layout constants in config.h keep the same screens usable on both OLED sizes.
// ascenders/descenders so nothing gets clipped by the visible window.

inline void drawRadarScreen(float sweepAngle, float rangeM, int flaggedInRange,
                             int totalActive, int untrustedActive,
                             bool allTrusted, bool muted, bool pulse,
                             bool newDevice,
                             float pingContactAngle = -1.0f) {
  u8g2.clearBuffer();
  u8g2.setFont(u8g2_font_4x6_tr);

  char hdr[24];
  snprintf(hdr, sizeof(hdr), "RADAR %.0fm", rangeM);
  u8g2.drawStr(OX(0), OY(6), hdr);

  const int cx = RADAR_CX, cy = RADAR_CY, maxR = RADAR_MAX_R;
  u8g2.drawCircle(OX(cx), OY(cy), maxR);
  u8g2.drawCircle(OX(cx), OY(cy), maxR / 2);
  u8g2.drawPixel(OX(cx), OY(cy));

  int sx = cx + (int)(cosf(sweepAngle) * maxR);
  int sy = cy + (int)(sinf(sweepAngle) * maxR);
  u8g2.drawLine(OX(cx), OY(cy), OX(sx), OY(sy));

  for (int i = 0; i < MAX_CANDIDATES; i++) {
    if (!g_candidates[i].active) continue;
    float ang = macToAngle(g_candidates[i].mac);
    int r = rssiToRadiusPx(g_candidates[i].rssi, maxR - 1);
    int bx = cx + (int)(cosf(ang) * r);
    int by = cy + (int)(sinf(ang) * r);
    bool pingContact = pulse && pingContactAngle >= 0.0f && fabsf(ang - pingContactAngle) < 0.001f;
    if (g_candidates[i].trusted) {
      u8g2.drawPixel(OX(bx), OY(by));   // trusted -> always a plain dot, never alerts
    } else if (g_candidates[i].score >= SCORE_FLAG_THRESHOLD) {
      u8g2.drawDisc(OX(bx), OY(by), pingContact ? 3 : 2);
      if (pingContact) u8g2.drawCircle(OX(bx), OY(by), 4);
    } else if (g_candidates[i].score >= SCORE_WATCH_THRESHOLD) {
      u8g2.drawCircle(OX(bx), OY(by), 1);
    } else {
      u8g2.drawPixel(OX(bx), OY(by));
    }
  }

  char info[20];
  if (newDevice) snprintf(info, sizeof(info), "NEW DEVICE");
  else if (flaggedInRange > 0) snprintf(info, sizeof(info), "ALERT %d", flaggedInRange);
  else if (allTrusted) snprintf(info, sizeof(info), "CLEAR");
  else if (untrustedActive > 0) snprintf(info, sizeof(info), "UNTRUSTED %d", untrustedActive);
  else snprintf(info, sizeof(info), pulse ? "SCAN ." : "SCAN...");
  u8g2.drawStr(OX(0), OY(14), info);
  snprintf(info, sizeof(info), "DEVICES %d", totalActive);
  u8g2.drawStr(OX(0), OY(21), info);
  u8g2.drawStr(OX(0), OY(28), muted ? "MUTED" : "LISTENING");

  u8g2.sendBuffer();
}

inline const char* sourceLabel(uint8_t src) {
  switch (src) {
    case SRC_WIFI_AP:  return "WiFi-AP";
    case SRC_WIFI_STA: return "WiFi-STA";
    case SRC_BLE:       return "BLE";
  }
  return "?";
}

inline void drawFinderScreen(bool haveTarget, const Candidate *c, bool unconfirmed, bool muted,
                              int posIdx, int posCount) {
  u8g2.clearBuffer();
  u8g2.setFont(u8g2_font_5x8_tr);

  if (!haveTarget) {
    u8g2.drawStr(OX(0), OY(8), muted ? "FINDER  MUTE" : "FINDER");
    u8g2.drawStr(OX(0), OY(20), "no target yet");
    u8g2.drawStr(OX(0), OY(31), "scanning...");
    u8g2.sendBuffer();
    return;
  }

  const char *tag = c->label[0] ? c->label : (c->hasName ? c->name : "unknown");
  char hdr[24];
  snprintf(hdr, sizeof(hdr), "FIND:%.6s%s", tag, c->trusted ? "*" : (unconfirmed ? "?" : ""));
  u8g2.drawStr(OX(0), OY(8), hdr);

  char rightTag[12];
  if (posCount > 0) snprintf(rightTag, sizeof(rightTag), "%s%d/%d", muted ? "M " : "", posIdx + 1, posCount);
  else snprintf(rightTag, sizeof(rightTag), "%s", muted ? "M" : "");
  if (rightTag[0]) {
    int rw = u8g2.getStrWidth(rightTag);
    u8g2.drawStr(OX(OLED_W - rw), OY(8), rightTag);
  }

  int barX = 4, barY = 9, barW = FINDER_BAR_W, barH = 7;
  u8g2.drawFrame(OX(barX), OY(barY), barW, barH);
  int fillPx = map(constrain(c->rssi, -95, -30), -95, -30, 0, barW - 2);
  if (fillPx > 0) u8g2.drawBox(OX(barX + 1), OY(barY + 1), fillPx, barH - 2);

  char dbmStr[24];
  snprintf(dbmStr, sizeof(dbmStr), "%ddBm  %s", c->rssi, sourceLabel(c->source));
  u8g2.drawStr(OX(0), OY(24), dbmStr);

  static int8_t prevRssi = -100;
  const char *trend;
  if (c->trusted) trend = "TRUSTED";
  else trend = (c->rssi > prevRssi + 1) ? "WARMER" : (c->rssi < prevRssi - 1) ? "colder" : "steady";
  prevRssi = c->rssi;
  u8g2.drawStr(OX(0), OY(31), trend);

  u8g2.sendBuffer();
}

// Known Devices picker: one device at a time, pot scrolls. Reused for both
// the trust toggle (Menu -> Known Devices) and calibration target selection.
inline void drawDevicePickScreen(const char *title, const char *footer,
                                  const Candidate *c, int idx, int count) {
  u8g2.clearBuffer();
  u8g2.setFont(u8g2_font_5x8_tr);
  u8g2.drawStr(OX(0), OY(8), title);

  char pos[12];
  snprintf(pos, sizeof(pos), "%d/%d", idx + 1, count);
  int pw = u8g2.getStrWidth(pos);
  u8g2.setFont(u8g2_font_4x6_tr);
  u8g2.drawStr(OX(OLED_W - pw), OY(6), pos);

  u8g2.setFont(u8g2_font_5x8_tr);
  const char *tag = c->label[0] ? c->label : (c->hasName ? c->name : "unknown");
  char l1[24];
  snprintf(l1, sizeof(l1), "%s%.11s", c->trusted ? "*" : " ", tag);
  u8g2.drawStr(OX(0), OY(18), l1);

  char l2[24];
  snprintf(l2, sizeof(l2), "%s  %ddBm", sourceLabel(c->source), c->rssi);
  u8g2.drawStr(OX(0), OY(26), l2);

  u8g2.setFont(u8g2_font_4x6_tr);
  u8g2.drawStr(OX(0), OY(31), footer);

  u8g2.sendBuffer();
}

// Single-item-at-a-time menu, driven by the pot: shows current item boxed
// and centered, plus an "n/N" position indicator so you always know where
// you are in the list.
inline void drawMenuScreen(const char *title, const char *itemText, int idx, int count) {
  u8g2.clearBuffer();
  u8g2.setFont(u8g2_font_4x6_tr);
  u8g2.drawStr(OX(0), OY(6), title);

  u8g2.setFont(u8g2_font_5x8_tr);
  int tw = u8g2.getStrWidth(itemText);
  int tx = (OLED_W - tw) / 2; if (tx < 2) tx = 2;
  int boxY0 = 10, boxH = 12;
  u8g2.drawFrame(OX(tx - 3), OY(boxY0), tw + 6, boxH);
  u8g2.drawStr(OX(tx), OY(20), itemText);

  char pos[20];
  snprintf(pos, sizeof(pos), "B1 NEXT  B2 OK  %d/%d", idx + 1, count);
  int pw = u8g2.getStrWidth(pos);
  int px = (OLED_W - pw) / 2; if (px < 0) px = 0;
  u8g2.setFont(u8g2_font_4x6_tr);
  u8g2.drawStr(OX(px), OY(31), pos);

  u8g2.sendBuffer();
}

// Generic up-to-4-line instruction screen, used by the calibration wizard.
inline void drawTextScreen(const char *l0, const char *l1, const char *l2, const char *l3) {
  u8g2.clearBuffer();
  u8g2.setFont(u8g2_font_5x8_tr);
  if (l0 && l0[0]) u8g2.drawStr(OX(0), OY(8), l0);
  if (l1 && l1[0]) u8g2.drawStr(OX(0), OY(16), l1);
  if (l2 && l2[0]) u8g2.drawStr(OX(0), OY(24), l2);
  if (l3 && l3[0]) u8g2.drawStr(OX(0), OY(31), l3);
  u8g2.sendBuffer();
}

inline void drawGameScreen(int lane, const int obstacleLane[], const int obstacleY[],
                           int score, bool started, bool finished) {
  u8g2.clearBuffer();
  u8g2.setFont(u8g2_font_5x8_tr);

  char header[24];
  snprintf(header, sizeof(header), "DRIVE  SCORE %d", score);
  u8g2.drawStr(OX(0), OY(8), header);

  if (finished) {
    u8g2.drawStr(OX(37), OY(20), "CRASH!");
    u8g2.drawStr(OX(0), OY(31), "B1=again hold B1=exit");
  } else if (!started) {
    u8g2.drawStr(OX(43), OY(20), "READY");
    u8g2.setFont(u8g2_font_4x6_tr);
    u8g2.drawStr(OX(0), OY(31), "B1 START / LEFT");
    u8g2.drawStr(OX(87), OY(31), "RIGHT B2");
  } else {
    const int laneX[3] = {22, 64, 106};
    u8g2.drawLine(OX(43), OY(10), OX(43), OY(28));
    u8g2.drawLine(OX(85), OY(10), OX(85), OY(28));
    u8g2.drawBox(OX(laneX[lane] - 5), OY(24), 10, 5);
    for (int i = 0; i < 3; i++) {
      if (obstacleY[i] >= -5 && obstacleY[i] <= 28) {
        u8g2.drawBox(OX(laneX[obstacleLane[i]] - 4), OY(obstacleY[i]), 8, 5);
      }
    }
    u8g2.setFont(u8g2_font_4x6_tr);
    u8g2.drawStr(OX(0), OY(31), "B1 LEFT");
    u8g2.drawStr(OX(89), OY(31), "RIGHT B2");
  }
  u8g2.sendBuffer();
}
