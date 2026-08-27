#pragma once
#include <NimBLEDevice.h>
#include "config.h"
#include "candidate_store.h"

inline bool macStringToBytes(const char *s, uint8_t *out) {
  unsigned int b[6];
  if (sscanf(s, "%x:%x:%x:%x:%x:%x", &b[0], &b[1], &b[2], &b[3], &b[4], &b[5]) != 6) return false;
  for (int i = 0; i < 6; i++) out[i] = (uint8_t)b[i];
  return true;
}

class DetectorBleCallbacks : public NimBLEScanCallbacks {
  void onResult(const NimBLEAdvertisedDevice *dev) override {
    uint8_t mac[6];
    if (!macStringToBytes(dev->getAddress().toString().c_str(), mac)) return;
    int8_t rssi = (int8_t)dev->getRSSI();
    const char *name = nullptr;
    String nm;
    if (dev->haveName()) { nm = dev->getName().c_str(); name = nm.c_str(); }
    candidateObserve(mac, rssi, SRC_BLE, 0, name);
  }
};

static NimBLEScan *g_bleScan = nullptr;
static DetectorBleCallbacks g_bleCallbacks;
static uint32_t g_lastBleKickMs = 0;

inline void bleScannerInit() {
  NimBLEDevice::init("");
  g_bleScan = NimBLEDevice::getScan();
  g_bleScan->setScanCallbacks(&g_bleCallbacks, false);
  g_bleScan->setActiveScan(true);
  g_bleScan->setInterval(97);
  g_bleScan->setWindow(37);
}

inline void bleScannerTick() {
  uint32_t now = millis();
  if (!g_bleScan->isScanning() && (now - g_lastBleKickMs) >= BLE_SCAN_PERIOD_MS) {
    g_bleScan->start(BLE_SCAN_DURATION_S, false);
    g_lastBleKickMs = now;
  }
}
