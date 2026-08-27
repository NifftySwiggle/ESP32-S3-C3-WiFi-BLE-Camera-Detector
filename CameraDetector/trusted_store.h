#pragma once
#include <Arduino.h>
#include <Preferences.h>
#include "config.h"

// A small persisted allow-list of MACs you've marked as "known good"
// (your own router, phone, smart plugs, etc). Trusted devices are still
// shown/tracked but never trigger alerts - this is the single biggest
// lever for cutting false positives, since a device that's always been
// there is far less interesting than one that just showed up.

struct TrustedStore {
  uint8_t count;
  uint8_t macs[MAX_TRUSTED][6];
};
static TrustedStore g_trusted;

inline void trustedLoad() {
  Preferences p;
  p.begin("camdet", true);
  size_t len = p.getBytesLength("trusted");
  if (len == sizeof(TrustedStore)) {
    p.getBytes("trusted", &g_trusted, sizeof(TrustedStore));
  } else {
    g_trusted.count = 0;
  }
  p.end();
}

inline void trustedSave() {
  Preferences p;
  p.begin("camdet", false);
  p.putBytes("trusted", &g_trusted, sizeof(TrustedStore));
  p.end();
}

inline bool trustedContains(const uint8_t mac[6]) {
  for (int i = 0; i < g_trusted.count; i++) {
    if (memcmp(g_trusted.macs[i], mac, 6) == 0) return true;
  }
  return false;
}

// Returns false only if the list is full (16 devices).
inline bool trustedAdd(const uint8_t mac[6]) {
  if (trustedContains(mac)) return true;
  if (g_trusted.count >= MAX_TRUSTED) return false;
  memcpy(g_trusted.macs[g_trusted.count], mac, 6);
  g_trusted.count++;
  trustedSave();
  return true;
}

inline bool trustedRemove(const uint8_t mac[6]) {
  for (int i = 0; i < g_trusted.count; i++) {
    if (memcmp(g_trusted.macs[i], mac, 6) != 0) continue;
    for (int j = i + 1; j < g_trusted.count; j++) {
      memcpy(g_trusted.macs[j - 1], g_trusted.macs[j], 6);
    }
    g_trusted.count--;
    trustedSave();
    return true;
  }
  return false;
}

inline void trustedClear() {
  g_trusted.count = 0;
  trustedSave();
}
