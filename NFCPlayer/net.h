#pragma once

#include <Arduino.h>

void netBegin();
void netLoop();

bool netStaConnected();
bool netApActive();
String netIp();
String netApSsid();
int netRssi();

bool netSaveWifi(const String& ssid, const String& pass);
void netReboot();
