#pragma once
#include <Arduino.h>
#include "config.h"

enum ButtonEvent { BTN_NONE, BTN_SHORT, BTN_LONG };

struct ButtonState {
  uint8_t     pin;
  bool        rawLast;
  bool        stablePressed;
  uint32_t    lastEdgeMs;
  uint32_t    pressStartMs;
  bool        longFired;
  ButtonEvent pending;
};

#define MAX_BUTTONS 2
static ButtonState g_buttons[MAX_BUTTONS];
static int g_numButtons = 0;

inline int buttonRegister(uint8_t pin) {
  pinMode(pin, INPUT_PULLUP);
  ButtonState &b = g_buttons[g_numButtons];
  b.pin = pin; b.rawLast = true; b.stablePressed = false;
  b.lastEdgeMs = millis(); b.pressStartMs = 0; b.longFired = false; b.pending = BTN_NONE;
  return g_numButtons++;
}

inline void buttonsTick() {
  uint32_t now = millis();
  for (int i = 0; i < g_numButtons; i++) {
    ButtonState &b = g_buttons[i];
    bool raw = digitalRead(b.pin);   // HIGH = released (pull-up), LOW = pressed
    if (raw != b.rawLast) { b.lastEdgeMs = now; b.rawLast = raw; }

    if (now - b.lastEdgeMs > DEBOUNCE_MS) {
      bool pressedNow = (raw == LOW);
      if (pressedNow && !b.stablePressed) {
        b.stablePressed = true;
        b.pressStartMs = now;
        b.longFired = false;
      } else if (!pressedNow && b.stablePressed) {
        b.stablePressed = false;
        if (!b.longFired) b.pending = BTN_SHORT;
      } else if (pressedNow && b.stablePressed && !b.longFired) {
        if (now - b.pressStartMs >= LONG_PRESS_MS) {
          b.longFired = true;
          b.pending = BTN_LONG;
        }
      }
    }
  }
}

inline ButtonEvent buttonGetEvent(int idx) {
  ButtonEvent e = g_buttons[idx].pending;
  g_buttons[idx].pending = BTN_NONE;
  return e;
}
