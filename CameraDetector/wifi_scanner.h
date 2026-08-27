#pragma once
#include <WiFi.h>
#include "esp_wifi.h"
#include "config.h"
#include "candidate_store.h"

// ---- lightweight ring buffer filled by the promiscuous callback ----
struct RawSighting {
  uint8_t mac[6];
  int8_t  rssi;
  uint8_t channel;
  uint8_t source;
  char    ssid[24];
  bool    hasSsid;
  bool    hiddenSsid;   // beacon seen with an empty SSID field
};
#define RING_SIZE 24
static RawSighting g_ring[RING_SIZE];
static volatile uint8_t g_ringHead = 0, g_ringTail = 0;

// Runs in the WiFi driver's task context - keep it fast, no allocation,
// no String use, and marked IRAM_ATTR so it's safe even if flash is busy.
static void IRAM_ATTR wifiSnifferCallback(void *buf, wifi_promiscuous_pkt_type_t type) {
  if (type != WIFI_PKT_MGMT && type != WIFI_PKT_DATA) return;
  const wifi_promiscuous_pkt_t *pkt = (const wifi_promiscuous_pkt_t *)buf;
  const uint8_t *payload = pkt->payload;
  const uint8_t *srcMac = payload + 10;   // 802.11 addr2 (transmitter) offset

  uint8_t nextHead = (g_ringHead + 1) % RING_SIZE;
  if (nextHead == g_ringTail) return;     // ring full, drop this one

  RawSighting s;
  memcpy(s.mac, srcMac, 6);
  s.rssi = pkt->rx_ctrl.rssi;
  s.channel = pkt->rx_ctrl.channel;
  s.hasSsid = false;
  s.hiddenSsid = false;
  s.ssid[0] = 0;
  s.source = (type == WIFI_PKT_DATA) ? SRC_WIFI_STA : SRC_WIFI_AP;

  if (type == WIFI_PKT_MGMT) {
    uint8_t subtype = payload[0] & 0xF0;
    if (subtype == 0x80 || subtype == 0x50) {   // beacon (0x80) or probe response (0x50)
      uint8_t ssidLen = payload[37];
      if (ssidLen > 0 && ssidLen < 24) {
        memcpy((void*)s.ssid, &payload[38], ssidLen);
        s.ssid[ssidLen] = 0;
        s.hasSsid = true;
      } else if (ssidLen == 0 && subtype == 0x80) {
        s.hiddenSsid = true;   // beacon with an empty SSID = hidden network
      }
    } else {
      return;   // skip auth/assoc/etc, we only want beacons/probe-resp/data
    }
  }

  g_ring[g_ringHead] = s;
  g_ringHead = nextHead;
}

enum WifiScanPhase { WSP_AP_SCAN, WSP_SWEEP, WSP_LOCKED };
static WifiScanPhase g_wifiPhase = WSP_AP_SCAN;
static uint32_t g_phaseStartMs = 0;
static uint8_t  g_sweepChannel = 1;
static uint32_t g_lastHopMs = 0;
static bool     g_promiscuousOn = false;
static uint8_t  g_extraDwells = 0;
#define MAX_EXTRA_DWELLS 2   // up to 3x dwell time on channels with a known candidate

inline void wifiPromiscuousEnable(bool en) {
  if (en == g_promiscuousOn) return;
  esp_wifi_set_promiscuous(en);
  g_promiscuousOn = en;
}

inline void wifiScannerInit() {
  WiFi.mode(WIFI_STA);
  WiFi.disconnect();
  wifi_promiscuous_filter_t filt = { .filter_mask = WIFI_PROMIS_FILTER_MASK_MGMT | WIFI_PROMIS_FILTER_MASK_DATA };
  esp_wifi_set_promiscuous_filter(&filt);
  esp_wifi_set_promiscuous_rx_cb(&wifiSnifferCallback);
  g_wifiPhase = WSP_AP_SCAN;
  g_phaseStartMs = millis();
  WiFi.scanNetworks(true, true);   // async, include hidden SSIDs
}

inline void wifiDrainRing() {
  while (g_ringTail != g_ringHead) {
    RawSighting s = g_ring[g_ringTail];   // volatile-safe copy-out
    g_ringTail = (g_ringTail + 1) % RING_SIZE;
    candidateObserve(s.mac, s.rssi, s.source, s.channel, s.hasSsid ? s.ssid : nullptr, s.hiddenSsid);
  }
}

inline void wifiHarvestApScanResults() {
  int n = WiFi.scanComplete();
  if (n < 0) return;
  for (int i = 0; i < n; i++) {
    uint8_t mac[6];
    memcpy(mac, WiFi.BSSID(i), 6);
    String ssid = WiFi.SSID(i);
    bool hidden = (ssid.length() == 0);
    candidateObserve(mac, (int8_t)WiFi.RSSI(i), SRC_WIFI_AP,
                     (uint8_t)WiFi.channel(i), ssid.length() ? ssid.c_str() : nullptr, hidden);
  }
  WiFi.scanDelete();
}

// finderLocked+finderChannel: if a Finder-mode target is active and its
// channel is known, park there instead of hopping, for fast RSSI updates.
inline void wifiScannerTick(bool finderLocked, uint8_t finderChannel) {
  wifiDrainRing();
  uint32_t now = millis();

  switch (g_wifiPhase) {
    case WSP_AP_SCAN: {
      int n = WiFi.scanComplete();
      if (n >= 0) {
        wifiHarvestApScanResults();
        wifiPromiscuousEnable(true);
        g_sweepChannel = 1;
        esp_wifi_set_channel(g_sweepChannel, WIFI_SECOND_CHAN_NONE);
        g_lastHopMs = now;
        g_wifiPhase = WSP_SWEEP;
      } else if (n == WIFI_SCAN_FAILED) {
        WiFi.scanNetworks(true, true);
      }
      break;
    }

    case WSP_SWEEP: {
      if (finderLocked && finderChannel > 0) {
        esp_wifi_set_channel(finderChannel, WIFI_SECOND_CHAN_NONE);
        g_wifiPhase = WSP_LOCKED;
        g_phaseStartMs = now;
        break;
      }
      if (now - g_lastHopMs >= WIFI_CHANNEL_DWELL_MS) {
        bool interesting = channelHasActiveCandidate(g_sweepChannel);
        if (interesting && g_extraDwells < MAX_EXTRA_DWELLS) {
          g_extraDwells++;          // linger here for better RSSI/activity samples
          g_lastHopMs = now;
        } else {
          g_extraDwells = 0;
          g_sweepChannel++;
          if (g_sweepChannel > 13) {
            wifiPromiscuousEnable(false);
            WiFi.scanNetworks(true, true);
            g_wifiPhase = WSP_AP_SCAN;
            g_sweepChannel = 1;
          } else {
            esp_wifi_set_channel(g_sweepChannel, WIFI_SECOND_CHAN_NONE);
          }
          g_lastHopMs = now;
        }
      }
      break;
    }

    case WSP_LOCKED: {
      if (!finderLocked) {
        g_wifiPhase = WSP_SWEEP;
        g_lastHopMs = now;
        break;
      }
      if (now - g_phaseStartMs > 4000) {   // periodically re-sweep everything
        g_wifiPhase = WSP_SWEEP;
        g_lastHopMs = now;
        g_sweepChannel = 1;
        esp_wifi_set_channel(g_sweepChannel, WIFI_SECOND_CHAN_NONE);
      }
      break;
    }
  }
}
