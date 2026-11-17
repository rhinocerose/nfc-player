#include "audio_player.h"

#include <Audio.h>
#include <SD.h>

#include "config.h"
#include "settings.h"

namespace {
Audio audio;

bool playing = false;
String currentPath;
volatile bool eofFlag = false;
void (*eofHandler)() = nullptr;

void onAudioInfo(Audio::msg_t msg) {
    if (msg.e == Audio::evt_eof) {
        eofFlag = true;
    }
}
}  // namespace

void audioBegin() {
    Audio::audio_info_callback = onAudioInfo;
    audio.setPinout(PIN_I2S_BCLK, PIN_I2S_LRC, PIN_I2S_DOUT);
    audio.setVolumeSteps(VOLUME_STEPS);
    audio.setVolume(settings().volume);
}

void audioLoop() {
    audio.loop();

    if (eofFlag) {
        eofFlag = false;
        if (playing) {
            playing = false;
            currentPath = "";
            if (eofHandler) eofHandler();
        }
    }
}

bool audioPlayFile(const String& fullPath) {
    if (fullPath.length() == 0) return false;
    if (!SD.exists(fullPath.c_str())) return false;

    audio.stopSong();
    playing = false;
    eofFlag = false;

    if (!audio.connecttoFS(SD, fullPath.c_str())) {
        currentPath = "";
        Serial.printf("[audio] cannot play %s\n", fullPath.c_str());
        return false;
    }

    currentPath = fullPath;
    playing = true;
    Serial.printf("[audio] playing %s\n", fullPath.c_str());
    return true;
}

void audioStop() {
    if (playing) audio.stopSong();
    playing = false;
    currentPath = "";
    eofFlag = false;
}

void audioSetVolume(uint8_t volume) {
    if (volume > MAX_VOLUME) volume = MAX_VOLUME;
    audio.setVolume(volume);
    settingsSetVolume(volume);
}

uint8_t audioGetVolume() {
    return settings().volume;
}

bool audioIsPlaying() {
    return playing;
}

String audioCurrentPath() {
    return currentPath;
}

uint32_t audioElapsedSeconds() {
    return playing ? audio.getAudioCurrentTime() : 0;
}

uint32_t audioDurationSeconds() {
    return playing ? audio.getAudioFileDuration() : 0;
}

void audioSetEofHandler(void (*handler)()) {
    eofHandler = handler;
}
