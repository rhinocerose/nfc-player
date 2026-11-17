#pragma once

#include <Arduino.h>

#include <vector>

struct CardMapping {
    String uid;
    String file;
    String label;
};

void cardsBegin();
bool cardsSave();

std::vector<CardMapping> cardsAll();
const CardMapping* cardsFind(const String& uid);

String cardsNormalizeUid(const String& raw);
bool cardsUpsert(const String& uid, const String& file, const String& label);
bool cardsRemove(const String& uid);
bool cardsRemoveByFile(const String& file);
