#include "player.h"

#include <SD.h>
#include <driver/gpio.h>
#include <esp_sleep.h>

#include "audio_player.h"
#include "cards.h"
#include "config.h"
#include "nfc_reader.h"
#include "status_led.h"
#include "storage.h"

namespace {
PlayerState state = PlayerState::IDLE;
String currentUid;
String currentFile;
uint32_t playStartedMs = 0;

void playError() {
    ledFlashError();
}

void onAudioEof() {
    state = PlayerState::IDLE;
    currentUid = "";
    currentFile = "";
    ledSetMode(LedMode::IDLE);
}

bool startFile(const String& fileName) {
    if (!storageFileExists(fileName)) {
        Serial.printf("[player] missing file: %s\n", fileName.c_str());
        return false;
    }

    if (!audioPlayFile(storagePath(fileName))) {
        Serial.printf("[player] playback failed: %s\n", fileName.c_str());
        return false;
    }

    currentFile = fileName;
    playStartedMs = millis();
    state = PlayerState::PLAYING;
    ledSetMode(LedMode::PLAYING);
    return true;
}
}  // namespace

void playerBegin() {
    nfcSetCardHandler(playerOnCard);
    audioSetEofHandler(onAudioEof);
}

void playerOnCard(const String& uid) {
    if (state == PlayerState::LEARNING) {
        // nfc_reader captures the UID during learn mode.
        return;
    }

    const CardMapping* mapping = cardsFind(uid);
    if (!mapping) {
        Serial.printf("[player] unknown card %s\n", uid.c_str());
        playError();
        return;
    }

    if (!storageFileExists(mapping->file)) {
        Serial.printf("[player] card %s points at missing file %s\n",
                      uid.c_str(), mapping->file.c_str());
        playError();
        return;
    }

    if (state == PlayerState::PLAYING && currentUid == uid) {
        playerStop();
        return;
    }

    if (state == PlayerState::PLAYING) {
        playerStop();
    }

    if (startFile(mapping->file)) {
        currentUid = uid;
        Serial.printf("[player] card %s -> %s\n", uid.c_str(), mapping->file.c_str());
    } else {
        playError();
    }
}

void playerStop() {
    audioStop();
    state = PlayerState::IDLE;
    currentUid = "";
    currentFile = "";
    ledSetMode(LedMode::IDLE);
}

bool playerPlayFile(const String& fileName) {
    if (state == PlayerState::LEARNING) {
        nfcLearnCancel();
        state = PlayerState::IDLE;
    }

    playerStop();
    if (!startFile(fileName)) {
        playError();
        return false;
    }

    currentUid = "";
    Serial.printf("[player] web play %s\n", fileName.c_str());
    return true;
}

void playerStartLearn() {
    if (state == PlayerState::PLAYING) playerStop();
    nfcLearnStart();
    state = PlayerState::LEARNING;
    ledSetMode(LedMode::LEARNING);
    Serial.println("[player] learn mode started");
}

void playerCancelLearn() {
    nfcLearnCancel();
    if (state == PlayerState::LEARNING) state = PlayerState::IDLE;
    ledSetMode(LedMode::IDLE);
    Serial.println("[player] learn mode cancelled");
}

bool playerLearnActive() {
    return state == PlayerState::LEARNING && nfcLearnActive();
}

String playerLearnUid() {
    return nfcLearnUid();
}

PlayerState playerState() {
    return state;
}

String playerCurrentFile() {
    return currentFile;
}

String playerCurrentUid() {
    return currentUid;
}

void playerLoop() {
    if (state == PlayerState::LEARNING && !nfcLearnActive()) {
        state = PlayerState::IDLE;
        ledSetMode(LedMode::IDLE);
    }

    if (state == PlayerState::PLAYING && !audioIsPlaying() &&
        (millis() - playStartedMs) > 500) {
        state = PlayerState::IDLE;
        currentUid = "";
        currentFile = "";
        ledSetMode(LedMode::IDLE);
    }
}

void playerShutdown() {
    Serial.println("[player] shutdown requested");
    playerStop();
    ledSetMode(LedMode::OFF);
    ledLoop();
    delay(200);

    SD.end();
    delay(100);

    esp_sleep_enable_ext0_wakeup(static_cast<gpio_num_t>(PIN_BTN_STOP), 0);
    esp_deep_sleep_start();
}
