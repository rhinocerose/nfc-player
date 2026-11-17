#include "status_led.h"

#include "config.h"

namespace {
LedMode mode = LedMode::BOOT;
LedMode baseMode = LedMode::IDLE;
uint32_t modeSince = 0;
uint32_t errorUntil = 0;
bool errorActive = false;

void writeLed(bool on) {
    digitalWrite(PIN_LED, LED_ACTIVE_HIGH ? (on ? HIGH : LOW) : (on ? LOW : HIGH));
}
}  // namespace

void ledBegin() {
    pinMode(PIN_LED, OUTPUT);
    writeLed(false);
    mode = LedMode::BOOT;
    baseMode = LedMode::BOOT;
    modeSince = millis();
}

void ledSetMode(LedMode newMode) {
    if (newMode != LedMode::ERROR) {
        baseMode = newMode;
        errorActive = false;
    }
    mode = newMode;
    modeSince = millis();
}

void ledFlashError() {
    errorActive = true;
    errorUntil = millis() + 1500;
    modeSince = millis();
}

void ledLoop() {
    const uint32_t now = millis();

    if (errorActive) {
        if (now >= errorUntil) {
            errorActive = false;
            mode = baseMode;
            modeSince = now;
            writeLed(false);
            return;
        }
        const uint32_t phase = (now - modeSince) / 150;
        if (phase >= 6) {
            writeLed(false);
        } else {
            writeLed((phase % 2) == 0);
        }
        return;
    }

    switch (mode) {
        case LedMode::OFF:
            writeLed(false);
            break;

        case LedMode::PLAYING:
            writeLed(true);
            break;

        case LedMode::BOOT:
        case LedMode::AP:
            writeLed(((now - modeSince) / 500) % 2 == 0);
            break;

        case LedMode::LEARNING:
            writeLed(((now - modeSince) / 120) % 2 == 0);
            break;

        case LedMode::ERROR:
            writeLed(((now - modeSince) / 120) % 2 == 0);
            break;

        case LedMode::IDLE:
        default: {
            const uint32_t cycle = (now - modeSince) % 4000;
            writeLed(cycle < 30);
            break;
        }
    }
}
