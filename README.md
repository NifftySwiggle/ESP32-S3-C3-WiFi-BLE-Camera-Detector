# 📡 Cyber-Detect RF Sweeper & Camera Detector

<div align="center">

![ESP32](https://img.shields.io/badge/Platform-ESP32--C3%20%7C%20ESP32--S3-blue?logo=espressif&logoColor=white)
![C++](https://img.shields.io/badge/Language-C%2B%2B%20%2F%20Arduino-00599C?logo=c%2B%2B&logoColor=white)
![Display](https://img.shields.io/badge/OLED-SSD1306%20(128x32%20%2F%2072x40)-brightgreen)
![Radio](https://img.shields.io/badge/Protocols-Wi--Fi%202.4GHz%20%2B%20BLE%205.0-orange)
![Version](https://img.shields.io/badge/Version-4.0.0-purple)

**A handheld 2.4 GHz RF & Hidden Camera Detector featuring a futuristic sci-fi cyberpunk HUD, promiscuous packet sniffing, BLE beacon discovery, Geiger proximity audio, and pure 2-button control.**

</div>

---

## 📑 Table of Contents

- [Overview](#-overview)
- [Key Features](#-key-features)
- [Hardware & Wiring](#-hardware--wiring)
  - [ESP32-S3 (0.91" 128x32 OLED)](#esp32-s3-091-128x32-oled)
  - [ESP32-C3 (0.42" 72x40 OLED)](#esp32-c3-042-72x40-oled)
- [2-Button Control Reference](#-2-button-control-reference)
- [Display Screens & Modes](#-display-screens--modes)
  - [1. Sci-Fi Radar Scope (`RADAR`)](#1-sci-fi-radar-scope-radar)
  - [2. Signal Homing Analyzer (`FINDER`)](#2-signal-homing-analyzer-finder)
  - [3. Cyberpunk System Menu (`MENU`)](#3-cyberpunk-system-menu-menu)
  - [4. Known Devices Manager](#4-known-devices-manager)
  - [5. RSSI Calibration Wizard](#5-rssi-calibration-wizard)
  - [6. "Spy Evader" Arcade Minigame](#6-spy-evader-arcade-minigame)
- [Detection Modes & Sensitivity](#-detection-modes--sensitivity)
  - [Detection Target Filters](#detection-target-filters)
  - [Sensitivity & Geiger Sniffing Levels](#sensitivity--geiger-sniffing-levels)
- [Quick Start & Installation](#-quick-start--installation)
  - [Dependencies](#dependencies)
  - [Flashing via Arduino IDE](#flashing-via-arduino-ide)
- [Project Architecture](#-project-architecture)
- [Technical Detection Methodology](#-technical-detection-methodology)
- [Limitations & Ethical Use](#-limitations--ethical-use)

---

## 🔍 Overview

The **Cyber-Detect RF Sweeper (v4)** turns compact ESP32-C3 or ESP32-S3 microcontrollers into handheld reconnaissance and counter-surveillance tools. By operating in **promiscuous Wi-Fi 802.11 mode** and running background **NimBLE Bluetooth Low Energy scans**, the device passively listens to 2.4 GHz radio frequency broadcasts without joining networks or injecting packets.

Designed from the ground up for streamlined **2-Button Operation**, it eliminates all potentiometer requirements while delivering a responsive, futuristic sci-fi HUD on standard SSD1306 monochrome OLEDs.

```
 +---------------------------------------------------------+
 | RAD[ALL] PIN       ! TARGETS: 02          .---.         |
 | DEV: 07 (02 UNTR)                         | / | <-- Sweeping
 | [B1]FIND [B2]SENS                         '---'     Scope
 +---------------------------------------------------------+
```

---

## ⚡ Key Features

- **🌐 Dual RF Scanning Core**:
  - **Wi-Fi 802.11 b/g/n Promiscuous Sniffer**: Captures Beacons, Probe Requests, Probe Responses, and Active Data Frames across channels 1–14 with adaptive channel hopping.
  - **NimBLE Bluetooth Low Energy Scanner**: Detects BLE beacons, AirTags/trackers, peripheral advertisements, and wearable devices.
- **🎯 Dual Target Filtering**:
  - `CAM ONLY`: Heuristic engine analyzing MAC OUI databases (Hikvision, Dahua, Wyze, Reolink, Axis, EZVIZ, etc.), suspicious SSID keywords (`spy`, `cam`, `hidden`), hidden network beacons, and traffic regularity.
  - `ALL DEV`: Sweeps and tracks **every** active 2.4 GHz emitter in range (routers, phones, laptops, IoT gadgets, BLE beacons).
- **🎚️ 4 Sensitivity Sniffing Profiles**:
  - Instant on-the-fly cycling with Button 2: `PINPOINT` (<0.5m), `HIGH` (~1.5m), `MEDIUM` (~4m), and `LOW` (~12m).
  - Rapid Geiger-counter proximity ticking in Pinpoint mode for sniffing outlets, smoke detectors, wall vents, and clocks.
- **🛰️ Sci-Fi Cyberpunk OLED UI**:
  - Rotating radar scope with cardinal alignment ticks (N/S/E/W), trailing phosphor beam line, and expanding target pulse rings.
  - Signal Homing Finder with segmented level meter, **Peak RSSI Hold Cursor (`▼`)**, and Doppler proximity trends (`WARMER ▲▲`, `COLDER ▼▼`).
  - Indexed card-style System Menu with live parameter boxes.
- **🛡️ Persistent Allow-Listing**:
  - One-click snapshot (`TrustAllNow`) creates an instant room baseline.
  - Allow-listed devices are saved to ESP32 Flash memory (`Preferences`) across reboots.
- **🎮 Built-In Arcade Minigame ("Spy Evader")**:
  - High-speed retro 3-lane cyberpunk runner with drone obstacles, bonus data packets (+5 pts), sound effects, and persistent high scores.
- **🔇 Non-Blocking SFX Engine**:
  - Multi-tone buzzer melodies, Geiger clicks, and synchronized status LED flashes. Mute mode silences the buzzer while preserving visual LED indicators.

---

## 🛠️ Hardware & Wiring

The firmware automatically configures pin mappings and display geometry based on the selected Arduino board target (`CONFIG_IDF_TARGET_ESP32C3` vs `CONFIG_IDF_TARGET_ESP32S3`).

### ESP32-S3 (0.91" 128x32 OLED)

| Component | ESP32-S3 Pin | Notes |
| :--- | :--- | :--- |
| **OLED VCC** | `3.3V` | 3.3V Logic & Power |
| **OLED GND** | `GND` | Ground |
| **OLED SDA** | `GPIO41` | I2C Data (Wire) |
| **OLED SCL** | `GPIO42` | I2C Clock (Wire) |
| **Passive Buzzer (+)** | `GPIO21` | Buzzer (-) connects to GND |
| **Status LED** | `GPIO48` | Onboard RGB LED (or external LED) |
| **Button 1 (Mode / Left)** | `GPIO16` | Connect to GND (Internal Pullup) |
| **Button 2 (Select / Right)** | `GPIO47` | Connect to GND (Internal Pullup) |

### ESP32-C3 (0.42" 72x40 OLED)

| Component | ESP32-C3 Pin | Notes |
| :--- | :--- | :--- |
| **OLED Display** | *Onboard* | Integrated 0.42" I2C OLED |
| **Passive Buzzer (+)** | `GPIO10` | Buzzer (-) connects to GND |
| **Status LED (Anode)** | `GPIO3` | Connect through 220–330Ω resistor to GND |
| **Button 1 (Mode / Left)** | `GPIO9` | Connect to GND (Internal Pullup) |
| **Button 2 (Select / Right)** | `GPIO0` | Connect to GND (Internal Pullup) |

> [!NOTE]
> All buttons utilize the ESP32's internal pull-up resistors (`INPUT_PULLUP`). Simply wire one side of each momentary button to the respective GPIO pin and the other side to `GND`. No external resistors required.

---

## 🎮 2-Button Control Reference

| Input Action | Radar Scope | Finder Tracker | System Menu | Spy Evader Game | Calibration Wizard |
| :--- | :--- | :--- | :--- | :--- | :--- |
| **Button 1 (Short Press)** | Switch to Finder | Switch to Radar | Scroll to Next Item | Move Left / Start | Advance to Next Step |
| **Button 1 (Long Press)** | Open System Menu | Open System Menu | Exit Menu to Radar | Exit to Menu | Cancel / Abort |
| **Button 2 (Short Press)** | Cycle Sensitivity Mode (`PIN`→`HI`→`MED`→`LOW`) | Cycle Target Device | Change Value / Toggle | Move Right / Start | Select Device / Adjust Distance |
| **Button 2 (Long Press)** | Toggle Audio Mute | Toggle **TRUST / UNTRUST** | Change Value / Toggle | *Unused* | *Unused* |

---

## 🖥️ Display Screens & Modes

### 1. Sci-Fi Radar Scope (`RADAR`)
The default surveillance sweep view:
- **Scope Reticle**: Outer & mid range rings, cardinal alignment ticks at North/South/East/West, and center crosshair (`+`).
- **Single Sweeping Ray**: Single crisp, precision scanning ray revolving smoothly around the radar reticle.
- **Dynamic Blips**:
  - **Untrusted Targets in Range**: Rendered as crisp 2x2 micro-dots with **expanding pulse rings** when swept across.
  - **Untrusted Background Signals**: Crisp circular dots.
  - **Trusted Baseline Devices**: Dim micro-pixels.
- **Live Telemetry Bar**: Active mode badge (`RAD[ALL]` vs `RAD[CAM]`), sensitivity indicator (`[PIN]`, `[HI]`, `[MED]`, `[LOW]`), inverted threat box (`! TGT: 02` / `STATUS: CLEAR`), total device counter, and audio mute status.

### 2. Signal Homing Analyzer (`FINDER`)
Locks onto the strongest (or selected) transmitter for physical search:
- **Toggle Trust / Untrust**: Long-press **Button 2** on any locked device to instantly mark it as **TRUSTED** (adding it to baseline) or **UNTRUSTED** (removing from baseline).
- **Segmented Signal Gauge**: Responsive bar with scale calibration divisions and a **Peak Hold Marker (`▼`)** showing the highest signal level observed during the sweep.
- **RF Telemetry**: Exact RSSI readout (`-38 dBm`), signal quality percentage (`[96%]`), and connection protocol (`WiFi-AP`, `WiFi-STA`, `BLE-ADV`).
- **Doppler Guidance**: Real-time trend analysis displaying `WARMER (+)` or `colder (-)` as you move closer or farther.
- **Geiger Sniffing**: In `PINPOINT` mode, produces rapid acoustic clicks with interval scaling down to 25ms near the target.

### 3. Cyberpunk System Menu (`MENU`)
Hold **Button 1** anywhere to enter:
- **Indexed Card Layout**: Shows current index (`/// SYSTEM MENU /// 03/11`) and divider rule.
- **Interactive Items**:
  - `OPERATING MODE` → `[ RADAR ]` / `[ FINDER ]`
  - `TARGET FILTER` → `[ ALL DEVICES ]` / `[ CAMERA ONLY ]`
  - `SENSITIVITY` → `[ PINPOINT ]` / `[ HIGH (1.5m) ]` / `[ MED (4.0m) ]` / `[ LOW (12m) ]`
  - `AUDIO VOLUME` → `[ HIGH (100%) ]` / `[ MED (70%) ]` / `[ LOW (30%) ]` / `[ MUTE (0%) ]`
  - `SPY EVADER` → `[ LAUNCH GAME ]`
  - `KNOWN DEVICES` → `[ BROWSE LIST ]`
  - `TRUST ALL NOW` → `[ SNAPSHOT BASELINE ]`
  - `CLEAR TRUST` → `[ RESET STORE ]`
  - `CALIBRATION` → `[ START WIZARD ]`
  - `RESET DEFAULTS` → `[ RESTORE FACTORY ]`
  - `EXIT MENU` → `[ RETURN >>> ]`

### 4. Known Devices Manager
Browse detected devices in the local airspace:
- Inspect MAC address, vendor/SSID label, source type (`WiFi-AP`, `WiFi-STA`, `BLE`), and RSSI.
- Press **Button 2** to instantly toggle trust status (`[*] TRUSTED` vs `[ ] UNTRUSTED`).

### 5. RSSI Calibration Wizard
Measures path-loss exponents specifically for your physical environment:
1. **Step 1/2**: Place a known 2.4 GHz reference device (e.g., phone hotspot) exactly **1.0 meter** away and sample RSSI.
2. **Step 2/2**: Move the device to a secondary distance (selectable: 2.0m–8.0m) and sample again.
3. Computes and saves the environment's exact Path Loss Exponent ($N$) and reference $1\text{m}$ RSSI ($\text{ref}$) into Flash memory.

### 6. "Spy Evader" Arcade Minigame
Built-in retro arcade driving game:
- **Responsive Screen Math**: Automatically adapts lane widths to both 128x32 (S3) and 72x40 (C3) screens.
- **Controls**: Button 1 (Left) and Button 2 (Right) to steer the cyber-car across 3 lanes.
- **Game Mechanics**: Dodge surveillance drones, collect flashing Data Packets (`+5 points`), dynamic speed acceleration, collision sound effects, and persistent high score tracking in Flash storage.

---

## 🎯 Detection Modes & Sensitivity

### Detection Target Filters

| Mode | Target Scope | Description |
| :--- | :--- | :--- |
| **`CAM ONLY`** | Surveillance Cameras | Compares MAC addresses against OUI lists (Hikvision, Dahua, Wyze, Reolink, Axis, Tuya, etc.), checks for keywords (`cam`, `spy`, `tapo`), hidden SSIDs, and regular frame transmissions. |
| **`ALL DEV`** | Full 2.4 GHz Spectrum | Tracks **all** active RF emitters. Untrusted routers, client laptops, mobile phones, BLE tags, and smart plugs trigger radar blips and homing locks. |

### Sensitivity & Geiger Sniffing Levels

| Profile | Range | RSSI Threshold | Ideal Use Case |
| :--- | :--- | :--- | :--- |
| **`PINPOINT`** | `< 0.5m` | `≥ -48 dBm` | Close-contact sniffing of wall outlets, smoke alarms, picture frames, vents, and clocks. Activates rapid Geiger clicks. |
| **`HIGH`** | `~ 1.5m` | `≥ -62 dBm` | Close-quarters room isolation. Pinpoints which desk, shelf, or corner contains the emitter without wall bleed. |
| **`MEDIUM`** | `~ 4.0m` | `≥ -74 dBm` | Standard room sweep coverage. |
| **`LOW`** | `~ 12.0m` | `≥ -86 dBm` | Long-range perimeter sweep detecting faint or through-wall signals. |

---

## 🚀 Quick Start & Installation

### Dependencies
Install the following libraries via the Arduino IDE Library Manager (**Sketch > Include Library > Manage Libraries...**):
1. **`U8g2`** by *oliver* (v2.35.x or newer) — Monochrome display graphics engine.
2. **`NimBLE-Arduino`** by *h2zero* (v1.4.x or newer) — Lightweight Bluetooth Low Energy stack.

### Flashing via Arduino IDE
1. Open Arduino IDE 2.x.
2. Ensure you have the **esp32** board package installed (**Tools > Board > Boards Manager** -> search `esp32` by Espressif).
3. Open `CameraDetector/CameraDetector.ino`.
4. Select your target board:
   - For ESP32-S3: **Tools > Board > ESP32S3 Dev Module** (USB CDC On Boot: *Enabled*).
   - For ESP32-C3: **Tools > Board > ESP32C3 Dev Module** (Flash Mode: *QIO / DIO*).
5. Connect your device via USB, choose the corresponding COM port, and click **Upload**.
6. Open the Serial Monitor at `115200` baud to confirm initialization.

---

## 📂 Project Architecture

```
cameradetectorv2/
├── CameraDetector/
│   ├── CameraDetector.ino    # Main application loop, state machine & 2-button input dispatcher
│   ├── config.h              # Pin definitions, board presets, thresholds & timing constants
│   ├── display_ui.h          # Cyberpunk HUD renderer, radar scope, finder gauge & arcade game
│   ├── alert_io.h            # Non-blocking audio SFX queue engine & LED flash controller
│   ├── candidate_store.h     # Dynamic candidate tracking, RSSI history & scoring engine
│   ├── oui_database.h        # Camera MAC OUI vendor signatures & suspicious keyword database
│   ├── wifi_scanner.h        # 802.11 promiscuous packet sniffer & channel hopper
│   ├── ble_scanner.h         # NimBLE advertisement observer & peripheral parser
│   ├── buttons.h             # Debounced multi-button engine (short/long press detection)
│   └── trusted_store.h       # Persistent allow-list storage engine using ESP32 Preferences
└── README.md                 # Project documentation
```

---

## 🔬 Technical Detection Methodology

### 1. Promiscuous 802.11 Frame Analysis
The ESP32 Wi-Fi hardware is configured in promiscuous mode with `WIFI_PROMIS_FILTER_MASK_ALL`. The sniffer callback processes:
- **Management Frames**: Beacons, Probe Requests, Probe Responses.
- **Data Frames**: Extracts Source Address (SA), Transmitter Address (TA), BSSID, and RSSI.
- **Channel Hopping**: Automatically rotates across 2.4 GHz channels 1 through 14 every 320ms (locking onto the target channel during Finder mode).

### 2. Heuristic Suspicion Scoring
In `CAM ONLY` mode, devices are evaluated with cumulative suspicion scoring:
$$\text{Score} = \text{OUI Score} + \text{Keyword Score} + \text{Hidden SSID Bonus} + \text{Signal Stability Bonus}$$
- Known Camera OUI match: $+40\text{ to }+60\text{ pts}$
- Generic IoT OUI match: $+15\text{ to }+25\text{ pts}$
- Suspicious SSID keywords (`spy`, `cam`, `hidden`, `tapo`, etc.): $+20\text{ to }+40\text{ pts}$
- Hidden SSID (zero-length broadcast): $+8\text{ pts}$
- Low RSSI variance ($\le 4\text{ dB}$ over recent samples): $+8\text{ pts}$
- Devices exceeding the `SCORE_FLAG_THRESHOLD` ($50\text{ pts}$) trigger immediate radar alert pings and homing priority.

---

## ⚠️ Limitations & Ethical Use

- **Passive RF Only**: Detects devices actively transmitting on 2.4 GHz Wi-Fi or BLE. Cannot detect wired, offline, or SD-card-only cameras.
- **5 GHz Networks**: Standard ESP32 hardware operates exclusively on 2.4 GHz bands and will not detect 5 GHz-only transmitters.
- **RF Distance Approximations**: RSSI signal strength varies depending on wall density, antenna polarization, and physical obstructions. Calibration improves distance modeling but is not a substitute for physical verification.
- **Ethical Notice**: This firmware is designed as an educational tool for inspecting environments you own or have explicit authorization to audit.

