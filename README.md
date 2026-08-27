# Camera Detector

An ESP32-C3 or ESP32-S3 device that passively detects nearby 2.4 GHz Wi-Fi and Bluetooth Low Energy (BLE) advertisements. It provides a radar view, signal finder, trusted-device list, calibration wizard, buzzer and LED alerts, and a small driving game.

It does not connect to networks, read private traffic, deauthenticate devices, crack passwords, or inject packets.

## Quick Start

1. Install Arduino IDE 2.x.
2. Install the Espressif ESP32 board package, `U8g2`, and `NimBLE-Arduino`.
3. Wire the board using the table for your board below.
4. Open `CameraDetector.ino` in Arduino IDE.
5. Select the matching ESP32-C3 or ESP32-S3 board and upload.
6. Open Serial Monitor at `115200` baud and confirm the OLED is detected at `0x3C`.

The detector starts in Radar mode. Let it scan for about a minute before deciding that a device is suspicious.

## Hardware

### ESP32-S3 with 0.91-inch 128x32 OLED

| Part | Connection |
| --- | --- |
| OLED VCC | 3.3V |
| OLED GND | GND |
| OLED SDA | GPIO41 |
| OLED SCL | GPIO42 |
| Passive buzzer | GPIO21 and GND |
| Onboard RGB status LED | GPIO48; flashes with the buzzer |
| Button 1 | GPIO16 to GND |
| Button 2 | GPIO47 to GND |
| Optional potentiometer wiper | GPIO1 |
| Potentiometer ends | 3.3V and GND |

### ESP32-C3 with onboard 0.42-inch 72x40 OLED

| Part | Connection |
| --- | --- |
| OLED | Onboard |
| Passive buzzer | GPIO10 and GND |
| LED anode through 220-330 ohm resistor | GPIO3 |
| LED cathode | GND |
| Button 1 | GPIO9 to GND |
| Button 2 | GPIO0 to GND |
| Optional potentiometer wiper | GPIO1 |
| Potentiometer ends | 3.3V and GND |

The potentiometer is optional. When installed, it controls the Radar range. Without it, the detector and button-driven menus still work, but the range cannot be adjusted.

Do not connect the OLED to 5V unless its module explicitly supports it. Keep all component grounds connected.

## Controls

| Input | Radar | Finder | Menu | Calibration |
| --- | --- | --- | --- | --- |
| Button 1 short | Switch to Finder | Switch to Radar | Next item | Advance |
| Button 1 long | Open menu | Open menu | Back or exit | Cancel |
| Button 2 short | Mute or unmute | Next device | Select item | Select device or increase distance |
| Button 2 long | Mute or unmute | Trust current device | Select item | Not used |
| Potentiometer | Adjust range | Not used | Not used | Not used |

## First Use

1. Turn on a nearby 2.4 GHz Wi-Fi access point, such as a phone hotspot. The ESP32 cannot detect 5 GHz-only networks.
2. Wait for devices to appear on the Radar screen.
3. Hold Button 1 to open the menu.
4. Use **Known Devices** to trust devices you recognize, or use **TrustAllNow** to create a clean baseline.
5. Return to Radar. `CLEAR` means all active devices are trusted. `UNTRUSTED` means at least one active device is not trusted.
6. Switch to Finder mode to follow one device while moving the detector around the room.

## Features

- **Radar:** displays nearby devices and alerts when flagged, untrusted devices are encountered.
- **Finder:** shows signal strength, device name, connection type, and signal trend.
- **Known Devices:** lets you trust or untrust individual devices.
- **Calibration:** measures a known device at two distances and saves a room-specific RSSI estimate.
- **Alerts:** buzzer and LED flash together. Muting the buzzer does not stop the visual LED alert.
- **Reaction Game:** a three-lane game in which Button 1 moves left, Button 2 moves right, and independently spawned obstacles fall in random lanes.

## How Detection Works

The detector listens for Wi-Fi and BLE advertisements that are already being broadcast. It combines several clues, including known camera manufacturers, camera-related names, signal stability, hidden network names, and observed traffic activity.

These clues are only indicators. A flagged device is not proof that a camera is present, and an unflagged device is not proof that a room is clear.

## What It Cannot Detect

- Wired, offline, or SD-card-only cameras
- Analog RF video transmitters
- 5 GHz-only Wi-Fi cameras
- Bluetooth Classic-only devices
- Devices using an unrecognized manufacturer or name
- A true direction or bearing; the radar position is a visual aid, not measured location

RSSI-based distance is approximate and affected by walls, orientation, reflections, and interference. Calibration can improve it but cannot make it a precision measurement.

## Radar and Alerts

The radar sweep completes a rotation in about 3.5 seconds. A new untrusted device causes an immediate alert. A flagged, untrusted device alerts when the sweep crosses its display position. Trusted devices remain visible but do not alert.

The display position is derived from the device address and is not a real bearing. For direction-finding, move or rotate the detector slowly and watch how the signal strength changes.

## Calibration

1. Open the menu with a long press of Button 1.
2. Select **Calibrate** with Button 2.
3. Select your known 2.4 GHz device from the detected-device list.
4. Place it exactly 1 meter from the detector and start the first sample.
5. Move the same device to the displayed second distance.
6. Use Button 2 to choose the distance, then Button 1 to take the second sample.
7. Press Button 1 to save the calibration. Hold Button 1 at any time to cancel.

Use a phone hotspot forced to 2.4 GHz as the known device. In a busy location, select it explicitly rather than assuming the strongest access point is yours.

## Limitations

This is an educational sweep tool, not a certified technical surveillance countermeasure (TSCM) instrument. Detection uses heuristics and can produce false positives and false negatives. Use it only in places you have permission to inspect.