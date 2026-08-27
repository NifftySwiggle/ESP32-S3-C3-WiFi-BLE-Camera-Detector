#pragma once
#include <Arduino.h>

// ============================================================
//  Known camera-related MAC OUIs + suspicious name keywords.
//  OUIs sourced from IEEE-registered vendor blocks (IPVM's
//  curated CCTV OUI list + individual vendor look-ups). This is
//  NOT exhaustive - treat matches as a hint, not certainty.
// ============================================================

struct OuiEntry {
  uint8_t b0, b1, b2;
  const char *vendor;
  int8_t weight;
};

// Dedicated video-surveillance / camera manufacturers -> strong signal
static const OuiEntry CAMERA_OUIS[] = {
  {0x00,0x18,0x85,"Avigilon",70},   {0x00,0x1F,0x92,"Avigilon",70},
  {0x00,0x40,0x8C,"Axis",70},       {0xAC,0xCC,0x8E,"Axis",70},
  {0x00,0x1A,0x07,"Arecont",70},
  {0x00,0x01,0x31,"Bosch",70},      {0x00,0x04,0x63,"Bosch",70},
  {0x00,0x1C,0x44,"Bosch",70},      {0x00,0x10,0x17,"Bosch",70},
  {0x00,0x1B,0x86,"Bosch",70},      {0x00,0x07,0x5F,"Bosch",70},
  {0x14,0xA7,0x8B,"Dahua",70},      {0x38,0xAF,0x29,"Dahua",70},
  {0x3C,0xEF,0x8C,"Dahua",70},      {0x4C,0x11,0xBF,"Dahua",70},
  {0x90,0x02,0xA9,"Dahua",70},      {0xBC,0x32,0x5F,"Dahua",70},
  {0xE0,0x50,0x8B,"Dahua",70},
  {0x00,0x1B,0xD8,"FLIR",70},       {0x00,0x40,0x7F,"FLIR",70},
  {0x00,0x13,0xE2,"Geovision",70},
  {0xE4,0x30,0x22,"Hanwha",70},     {0x00,0x09,0x18,"Hanwha",70},
  {0x18,0x68,0xCB,"Hikvision",70},  {0x28,0x57,0xBE,"Hikvision",70},
  {0x44,0x19,0xB6,"Hikvision",70},  {0x4C,0xBD,0x8F,"Hikvision",70},
  {0x54,0xC4,0x15,"Hikvision",70},  {0x64,0xDB,0x8B,"Hikvision",70},
  {0x94,0xE1,0xAC,"Hikvision",70},  {0xA4,0x14,0x37,"Hikvision",70},
  {0xB4,0xA3,0x82,"Hikvision",70},  {0xBC,0xAD,0x28,"Hikvision",70},
  {0xC0,0x56,0xE3,"Hikvision",70},  {0xC4,0x2F,0x90,"Hikvision",70},
  {0x00,0x0A,0x13,"Honeywell",70},
  {0x14,0x2F,0xFD,"LTS",70},
  {0x00,0x10,0xBE,"MarchNtwk",70},  {0x00,0x12,0x81,"MarchNtwk",70},
  {0x00,0x03,0xC5,"Mobotix",70},
  {0x00,0x04,0x7D,"Pelco",70},
  {0x00,0x1C,0x27,"Sunell",70},
  {0x00,0x02,0xD1,"Vivotek",70},
  {0x48,0xEA,0x63,"Uniview",70},
  {0x2C,0xAA,0x8E,"Wyze",75},       {0x7C,0x78,0xB2,"Wyze",75},
  {0x80,0x48,0x2C,"Wyze",75},       {0xD0,0x3F,0x27,"Wyze",75},
  {0xF0,0xC8,0x8B,"Wyze",75},
  {0xEC,0x71,0xDB,"Reolink",75},
  {0x9C,0x8E,0xCD,"Amcrest",75},
  {0x0C,0xA6,0x4C,"EZVIZ",75},      {0x20,0xBB,0xBC,"EZVIZ",75},
  {0x34,0xC6,0xDD,"EZVIZ",75},      {0x54,0xD6,0x0D,"EZVIZ",75},
  {0x58,0x8F,0xCF,"EZVIZ",75},      {0x64,0x24,0x4D,"EZVIZ",75},
  {0x64,0xF2,0xFB,"EZVIZ",75},      {0x78,0xA6,0xA0,"EZVIZ",75},
  {0x78,0xC1,0xAE,"EZVIZ",75},      {0x94,0xEC,0x13,"EZVIZ",75},
  {0xAC,0x1C,0x26,"EZVIZ",75},      {0xEC,0x97,0xE0,"EZVIZ",75},
  {0xF4,0x70,0x18,"EZVIZ",75},      {0xFC,0x24,0x22,"EZVIZ",75},
};
static const int NUM_CAMERA_OUIS = sizeof(CAMERA_OUIS) / sizeof(CAMERA_OUIS[0]);

// Generic WiFi/IoT chip silicon -- used by a huge % of unbranded
// AliExpress "spy cams" but ALSO by smart bulbs/plugs/sensors that
// aren't cameras at all, so this alone is only a weak hint.
static const OuiEntry GENERIC_IOT_OUIS[] = {
  {0x00,0x4B,0x12,"Espressif",15}, {0x00,0x70,0x07,"Espressif",15},
  {0x04,0x83,0x08,"Espressif",15}, {0x04,0xB2,0x47,"Espressif",15},
  {0x08,0x3A,0x8D,"Espressif",15}, {0x08,0x3A,0xF2,"Espressif",15},
  {0x08,0x92,0x72,"Espressif",15}, {0x08,0xA6,0xF7,"Espressif",15},
  {0x08,0xAD,0x0A,"Espressif",15}, {0x08,0xB6,0x1F,"Espressif",15},
  {0x08,0xD1,0xF9,"Espressif",15}, {0x08,0xF9,0xE0,"Espressif",15},
  {0x0C,0x4E,0xA0,"Espressif",15}, {0x0C,0x8B,0x95,"Espressif",15},
  {0x0C,0xB8,0x15,"Espressif",15}, {0x0C,0xDC,0x7E,"Espressif",15},
  {0x10,0x00,0x3B,"Espressif",15}, {0x7C,0x9E,0xBD,"Espressif",15},
};
static const int NUM_GENERIC_OUIS = sizeof(GENERIC_IOT_OUIS) / sizeof(GENERIC_IOT_OUIS[0]);

// Strong keywords: rarely appear in SSID/BLE names for any reason
// other than a camera / covert device announcing itself.
static const char* KEYWORDS_STRONG[] = {
  "spy","hidden","nanny","covert","spycam","peephole",
  "wyze","tapo","ezviz","hikvision","dahua","reolink","foscam",
  "eufy","annke","imou","victure","netvue","wansview","blurams",
  "mijia","yi home","yi iot","prov_",
};
static const int NUM_KEYWORDS_STRONG = sizeof(KEYWORDS_STRONG) / sizeof(KEYWORDS_STRONG[0]);

// Moderate keywords: common camera/DVR words that also show up in
// unrelated SSIDs sometimes, so they count for less on their own.
static const char* KEYWORDS_MODERATE[] = {
  "cam","ipcam","webcam","dvr","nvr","onvif","p2p_",
};
static const int NUM_KEYWORDS_MODERATE = sizeof(KEYWORDS_MODERATE) / sizeof(KEYWORDS_MODERATE[0]);

// Look up a MAC's OUI. Returns suspicion weight (0 if unknown) and
// fills vendorOut with the matched vendor name (or NULL).
inline int8_t ouiLookup(const uint8_t mac[6], const char **vendorOut) {
  for (int i = 0; i < NUM_CAMERA_OUIS; i++) {
    const OuiEntry &e = CAMERA_OUIS[i];
    if (mac[0] == e.b0 && mac[1] == e.b1 && mac[2] == e.b2) { *vendorOut = e.vendor; return e.weight; }
  }
  for (int i = 0; i < NUM_GENERIC_OUIS; i++) {
    const OuiEntry &e = GENERIC_IOT_OUIS[i];
    if (mac[0] == e.b0 && mac[1] == e.b1 && mac[2] == e.b2) { *vendorOut = e.vendor; return e.weight; }
  }
  *vendorOut = nullptr;
  return 0;
}

// Score a name/SSID string against the keyword tables.
inline int16_t keywordScore(const char *name) {
  if (!name || !name[0]) return 0;
  String s(name);
  s.toLowerCase();
  for (int i = 0; i < NUM_KEYWORDS_STRONG; i++) {
    if (s.indexOf(KEYWORDS_STRONG[i]) >= 0) return 55;
  }
  for (int i = 0; i < NUM_KEYWORDS_MODERATE; i++) {
    if (s.indexOf(KEYWORDS_MODERATE[i]) >= 0) return 30;
  }
  return 0;
}
