#pragma once

#include <Arduino.h>

#include "config.h"

struct Settings {
    String wifiSsid;
    String wifiPass;
    String apPass;
    String webPass;
    String deviceName;
    uint8_t volume = DEFAULT_VOLUME;
};

Settings& settings();
void settingsBegin();
bool settingsSave();

void settingsSetWifi(const String& ssid, const String& pass);
void settingsClearWifi();
void settingsSetVolume(uint8_t volume);

String settingsSanitizeDeviceName(const String& raw);
