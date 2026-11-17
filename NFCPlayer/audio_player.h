#pragma once

#include <Arduino.h>

void audioBegin();
void audioLoop();

bool audioPlayFile(const String& fullPath);
void audioStop();

void audioSetVolume(uint8_t volume);
uint8_t audioGetVolume();

bool audioIsPlaying();
String audioCurrentPath();
uint32_t audioElapsedSeconds();
uint32_t audioDurationSeconds();

void audioSetEofHandler(void (*handler)());
