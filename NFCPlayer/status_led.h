#pragma once

#include <Arduino.h>

enum class LedMode {
    OFF,
    BOOT,
    IDLE,
    PLAYING,
    LEARNING,
    AP,
    ERROR,
};

void ledBegin();
void ledSetMode(LedMode mode);
void ledFlashError();
void ledLoop();
