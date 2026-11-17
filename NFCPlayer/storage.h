#pragma once

#include <Arduino.h>

#include <vector>

struct AudioFileInfo {
    String name;
    size_t size = 0;
};

bool storageBegin();
bool storageReady();

uint64_t storageTotalBytes();
uint64_t storageFreeBytes();

bool storageIsAudioName(const String& name);
String storageSanitizeName(const String& raw);
String storageUniqueName(const String& sanitizedName);
String storagePath(const String& name);
String storageTempPath();

bool storageFileExists(const String& name);
bool storageDeleteFile(const String& name);
bool storageCommit(const String& tempPath, const String& finalName);
void storageCleanupTemp();

std::vector<AudioFileInfo> storageListAudio();
