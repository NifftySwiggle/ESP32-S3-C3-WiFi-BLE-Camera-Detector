#pragma once
#include <Arduino.h>
#include "config.h"
#include "oui_database.h"
#include "trusted_store.h"

enum SourceType : uint8_t { SRC_WIFI_AP = 0, SRC_WIFI_STA = 1, SRC_BLE = 2 };

struct Candidate {
  bool     active;
  uint8_t  mac[6];
  int8_t   rssi;                        // median-filtered
  int8_t   rssiHistory[RSSI_HIST_LEN];
  uint8_t  rssiHistCount;
  uint8_t  rssiHistIdx;
  int8_t   rssiVariance;                // recent spread (max-min), rough steadiness measure
  uint32_t firstSeenMs;
  uint32_t lastSeenMs;
  uint8_t  source;
  uint8_t  channel;                     // 0 = unknown
  int16_t  ouiScore;
  int16_t  keywordScoreVal;
  bool     sawHidden;                   // beacon seen with an empty (hidden) SSID
  bool     activeTraffic;               // sustained data-frame rate -> possible streaming
  bool     randomized;                  // locally-administered/private MAC (likely a phone)
  bool     trusted;                     // on the saved allow-list
  int16_t  score;                       // final suspicion score - what everything else reads
  uint16_t staObsCount;
  uint16_t staObsBaseline;
  uint32_t activityCheckMs;
  char     label[16];                   // vendor name, or "" if unknown
  char     name[24];                    // SSID / BLE name, or "" if none
  bool     hasName;
};

#define ACTIVITY_BONUS      15
#define ACTIVITY_WINDOW_MS  4000UL
#define ACTIVITY_MIN_DELTA  8      // ~2+ observed data frames/sec -> looks "busy"

static Candidate g_candidates[MAX_CANDIDATES];
static uint32_t g_newCandidateMask = 0;

inline void candidateStoreInit() {
  memset(g_candidates, 0, sizeof(g_candidates));
}

inline int candidateFind(const uint8_t mac[6]) {
  for (int i = 0; i < MAX_CANDIDATES; i++) {
    if (g_candidates[i].active && memcmp(g_candidates[i].mac, mac, 6) == 0) return i;
  }
  return -1;
}

inline int candidateAllocSlot() {
  for (int i = 0; i < MAX_CANDIDATES; i++) if (!g_candidates[i].active) return i;
  int worst = 0;
  for (int i = 1; i < MAX_CANDIDATES; i++) {
    if (g_candidates[i].lastSeenMs < g_candidates[worst].lastSeenMs) worst = i;
  }
  return worst;
}

inline int8_t medianOfN(const int8_t *src, int n) {
  int8_t tmp[RSSI_HIST_LEN];
  for (int i = 0; i < n; i++) tmp[i] = src[i];
  for (int i = 1; i < n; i++) {           // small insertion sort, n<=5
    int8_t key = tmp[i]; int j = i - 1;
    while (j >= 0 && tmp[j] > key) { tmp[j + 1] = tmp[j]; j--; }
    tmp[j + 1] = key;
  }
  return tmp[n / 2];
}

inline void candidateRecomputeScore(Candidate &c) {
  int16_t oui = c.ouiScore;
  if (c.randomized) oui /= RANDOMIZED_MAC_OUI_DIV;   // a real OUI match is far less likely to
                                                       // mean anything if the address is private
  int16_t s = oui + c.keywordScoreVal;
  if (c.activeTraffic) s += ACTIVITY_BONUS;
  if (c.sawHidden) s += HIDDEN_SSID_BONUS;
  if (c.rssiHistCount >= RSSI_HIST_LEN && c.rssiVariance <= STEADY_VARIANCE_DB) s += STEADY_BONUS;
  if (c.randomized) s = (int16_t)(s * RANDOMIZED_MAC_SCALE_10 / 10);   // phones/laptops randomize; cameras don't
  c.score = s;
}

// Record (or refresh) a sighting. name may be NULL if unknown.
// hiddenSsid: true if we saw a beacon for this device with an empty SSID field.
inline void candidateObserve(const uint8_t mac[6], int8_t rssi, uint8_t source,
                              uint8_t channel, const char *name, bool hiddenSsid = false) {
  uint32_t now = millis();
  int idx = candidateFind(mac);
  bool isNew = (idx < 0);
  if (isNew) idx = candidateAllocSlot();

  Candidate &c = g_candidates[idx];
  if (isNew) {
    memset(&c, 0, sizeof(c));
    c.active = true;
    memcpy(c.mac, mac, 6);
    c.firstSeenMs = now;
    c.activityCheckMs = now;
    c.randomized = (mac[0] & 0x02) != 0;   // U/L bit set = locally administered / private address
    g_newCandidateMask |= (1UL << idx);
  }
  c.rssiHistory[c.rssiHistIdx] = rssi;
  c.rssiHistIdx = (c.rssiHistIdx + 1) % RSSI_HIST_LEN;
  if (c.rssiHistCount < RSSI_HIST_LEN) c.rssiHistCount++;
  c.rssi = medianOfN(c.rssiHistory, c.rssiHistCount);
  int8_t mn = c.rssiHistory[0], mx = c.rssiHistory[0];
  for (int i = 1; i < c.rssiHistCount; i++) {
    if (c.rssiHistory[i] < mn) mn = c.rssiHistory[i];
    if (c.rssiHistory[i] > mx) mx = c.rssiHistory[i];
  }
  c.rssiVariance = mx - mn;

  c.lastSeenMs = now;
  c.source = source;
  if (channel) c.channel = channel;
  if (source == SRC_WIFI_STA) c.staObsCount++;
  if (hiddenSsid) c.sawHidden = true;
  if (name && name[0]) {
    strncpy(c.name, name, sizeof(c.name) - 1);
    c.name[sizeof(c.name) - 1] = 0;
    c.hasName = true;
  }

  const char *vendor = nullptr;
  c.ouiScore = ouiLookup(c.mac, &vendor);
  c.keywordScoreVal = keywordScore(c.hasName ? c.name : nullptr);
  c.trusted = trustedContains(c.mac);
  candidateRecomputeScore(c);
  if (vendor) {
    strncpy(c.label, vendor, sizeof(c.label) - 1);
    c.label[sizeof(c.label) - 1] = 0;
  }

  if (isNew && c.score >= SCORE_WATCH_THRESHOLD) {
    Serial.printf("[detect] %02X:%02X:%02X:%02X:%02X:%02X rssi=%d score=%d rand=%d %s %s\n",
                  mac[0], mac[1], mac[2], mac[3], mac[4], mac[5],
                  rssi, c.score, c.randomized, vendor ? vendor : "-", c.hasName ? c.name : "");
  }
}

inline bool candidateTakeNewEvent(uint8_t mac[6], int8_t &rssi) {
  for (int i = 0; i < MAX_CANDIDATES; i++) {
    uint32_t bit = (1UL << i);
    if ((g_newCandidateMask & bit) == 0) continue;
    g_newCandidateMask &= ~bit;
    if (!g_candidates[i].active) continue;
    memcpy(mac, g_candidates[i].mac, 6);
    rssi = g_candidates[i].rssi;
    return true;
  }
  return false;
}

inline bool candidateAllTrusted() {
  bool found = false;
  for (int i = 0; i < MAX_CANDIDATES; i++) {
    if (!g_candidates[i].active) continue;
    found = true;
    if (!g_candidates[i].trusted) return false;
  }
  return found;
}

inline void candidateStorePrune(uint32_t nowMs) {
  for (int i = 0; i < MAX_CANDIDATES; i++) {
    Candidate &c = g_candidates[i];
    if (!c.active) continue;
    if (nowMs - c.lastSeenMs > STALE_TIMEOUT_MS) { c.active = false; continue; }
    if (nowMs - c.activityCheckMs >= ACTIVITY_WINDOW_MS) {
      uint16_t delta = c.staObsCount - c.staObsBaseline;
      c.activeTraffic = (delta >= ACTIVITY_MIN_DELTA);
      c.staObsBaseline = c.staObsCount;
      c.activityCheckMs = nowMs;
      candidateRecomputeScore(c);
    }
  }
}

inline int candidateStrongest(int16_t minScore, bool excludeTrusted = false) {
  int best = -1;
  for (int i = 0; i < MAX_CANDIDATES; i++) {
    if (!g_candidates[i].active) continue;
    if (g_candidates[i].score < minScore) continue;
    if (excludeTrusted && g_candidates[i].trusted) continue;
    if (best < 0 || g_candidates[i].rssi > g_candidates[best].rssi) best = i;
  }
  return best;
}

inline int candidateStrongestFilter(TargetFilter filter, bool excludeTrusted = false) {
  if (filter == TARGET_CAM_ONLY) {
    int idx = candidateStrongest(SCORE_FLAG_THRESHOLD, excludeTrusted);
    if (idx < 0) idx = candidateStrongest(0, excludeTrusted);
    return idx;
  } else {
    return candidateStrongest(0, excludeTrusted);
  }
}


inline int candidateCountActive() {
  int n = 0;
  for (int i = 0; i < MAX_CANDIDATES; i++) if (g_candidates[i].active) n++;
  return n;
}

inline int candidateBuildActiveList(int *indices, int capacity) {
  int count = 0;
  for (int i = 0; i < MAX_CANDIDATES && count < capacity; i++) {
    if (g_candidates[i].active) indices[count++] = i;
  }
  return count;
}

inline bool channelHasActiveCandidate(uint8_t ch) {
  for (int i = 0; i < MAX_CANDIDATES; i++) {
    if (g_candidates[i].active && g_candidates[i].channel == ch) return true;
  }
  return false;
}

// Marks every currently-active device as trusted (a "baseline snapshot").
inline void trustedSnapshotAll() {
  for (int i = 0; i < MAX_CANDIDATES; i++) {
    if (g_candidates[i].active) trustedAdd(g_candidates[i].mac);
  }
  for (int i = 0; i < MAX_CANDIDATES; i++) {
    if (!g_candidates[i].active) continue;
    g_candidates[i].trusted = trustedContains(g_candidates[i].mac);
    candidateRecomputeScore(g_candidates[i]);
  }
}
