#include <Arduino.h>
#include <esp32-hal-psram.h>

#include "audio_player.h"
#include "buttons.h"
#include "cards.h"
#include "config.h"
#include "net.h"
#include "nfc_reader.h"
#include "player.h"
#include "settings.h"
#include "status_led.h"
#include "storage.h"
#include "web_server.h"

namespace {
LedMode lastBaseMode = LedMode::BOOT;

LedMode desiredBaseMode() {
    switch (playerState()) {
        case PlayerState::PLAYING:
            return LedMode::PLAYING;
        case PlayerState::LEARNING:
            return LedMode::LEARNING;
        default:
            return netApActive() ? LedMode::AP : LedMode::IDLE;
    }
}
}  // namespace

void setup() {
    Serial.begin(115200);
    delay(200);
    Serial.println();
    Serial.printf("=== %s %s ===\n", FW_NAME, FW_VERSION);

    if (!psramFound()) {
        Serial.println("[boot] ERROR: no PSRAM found; audio decoding will not work");
    } else {
        Serial.printf("[boot] PSRAM: %u bytes\n", static_cast<unsigned>(ESP.getPsramSize()));
    }
    Serial.printf("[boot] free heap: %u bytes\n", static_cast<unsigned>(ESP.getFreeHeap()));

    settingsBegin();
    ledBegin();

    const bool sdOk = storageBegin();
    if (!sdOk) {
        Serial.println("[boot] SD card is not available");
    }

    cardsBegin();
    netBegin();
    webBegin();

    audioBegin();
    nfcBegin();

    buttonsBegin();
    buttonsSetHandlers(playerStop, playerShutdown);
    playerBegin();

    lastBaseMode = desiredBaseMode();
    ledSetMode(lastBaseMode);
    if (!sdOk || !psramFound()) ledFlashError();

    Serial.printf("[boot] ready: %s  http://%s/\n",
                  netApActive() ? "AP mode" : "station mode", netIp().c_str());
    if (netApActive()) {
        Serial.printf("[boot] join AP '%s' with password '%s'\n",
                      netApSsid().c_str(), settings().apPass.c_str());
    }
}

void loop() {
    audioLoop();
    nfcLoop();
    buttonsLoop();
    playerLoop();
    netLoop();
    webLoop();

    const LedMode desired = desiredBaseMode();
    if (desired != lastBaseMode) {
        lastBaseMode = desired;
        ledSetMode(desired);
    }
    ledLoop();

    delay(1);
}
