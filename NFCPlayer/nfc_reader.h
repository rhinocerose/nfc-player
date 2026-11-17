#pragma once

#include <Arduino.h>

void nfcBegin();
void nfcLoop();
bool nfcReady();

void nfcSetCardHandler(void (*handler)(const String& uid));

void nfcLearnStart();
void nfcLearnCancel();
bool nfcLearnActive();
String nfcLearnUid();
