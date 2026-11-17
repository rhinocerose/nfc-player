#include "buttons.h"

#include "config.h"

namespace {
struct Button {
    uint8_t pin;
    bool stable;
    bool raw;
    uint32_t lastChange;
    uint32_t pressedAt;
    bool longFired;
};

Button stopButton{PIN_BTN_STOP, true, true, 0, 0, false};
Button spareButton{PIN_BTN_SPARE, true, true, 0, 0, false};

void (*shortHandler)() = nullptr;
void (*longHandler)() = nullptr;

void beginButton(Button& button) {
    pinMode(button.pin, INPUT_PULLUP);
    button.raw = digitalRead(button.pin);
    button.stable = button.raw;
    button.lastChange = millis();
    button.pressedAt = 0;
    button.longFired = false;
}

void pollStopButton(uint32_t now) {
    const bool reading = digitalRead(stopButton.pin);
    if (reading != stopButton.raw) {
        stopButton.raw = reading;
        stopButton.lastChange = now;
    }

    if ((now - stopButton.lastChange) < BUTTON_DEBOUNCE_MS) return;
    if (reading == stopButton.stable) {
        // Still held: fire the long-press action once.
        if (stopButton.stable == LOW && !stopButton.longFired &&
            (now - stopButton.pressedAt) >= BUTTON_LONG_MS) {
            stopButton.longFired = true;
            if (longHandler) longHandler();
        }
        return;
    }

    stopButton.stable = reading;
    if (stopButton.stable == LOW) {
        stopButton.pressedAt = now;
        stopButton.longFired = false;
    } else if (!stopButton.longFired && shortHandler &&
               (now - stopButton.pressedAt) < BUTTON_LONG_MS) {
        shortHandler();
    }
}

void pollSpareButton(uint32_t now) {
    const bool reading = digitalRead(spareButton.pin);
    if (reading != spareButton.raw) {
        spareButton.raw = reading;
        spareButton.lastChange = now;
    }

    if ((now - spareButton.lastChange) < BUTTON_DEBOUNCE_MS) return;
    if (reading == spareButton.stable) return;

    spareButton.stable = reading;
    if (spareButton.stable == LOW) {
        Serial.println("[btn] button 2 pressed (unassigned)");
    }
}
}  // namespace

void buttonsBegin() {
    beginButton(stopButton);
    beginButton(spareButton);
}

void buttonsSetHandlers(void (*shortPress)(), void (*longPress)()) {
    shortHandler = shortPress;
    longHandler = longPress;
}

void buttonsLoop() {
    const uint32_t now = millis();
    pollStopButton(now);
    pollSpareButton(now);
}
