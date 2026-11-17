#pragma once

#include <Arduino.h>

enum class PlayerState {
    IDLE,
    PLAYING,
    LEARNING,
};

void playerBegin();
void playerLoop();

void playerOnCard(const String& uid);
void playerStop();
bool playerPlayFile(const String& fileName);

void playerStartLearn();
void playerCancelLearn();
bool playerLearnActive();
String playerLearnUid();

PlayerState playerState();
String playerCurrentFile();
String playerCurrentUid();

void playerShutdown();
