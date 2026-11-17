#include "storage.h"

#include <SD.h>
#include <SPI.h>

#include <algorithm>
#include <cctype>

#include "config.h"

namespace {
bool sdReady = false;

String pathFor(const String& name) {
    return String(AUDIO_DIR) + "/" + name;
}

bool hasAudioExtension(const String& lowerName) {
    for (size_t i = 0; i < AUDIO_EXTENSION_COUNT; ++i) {
        String suffix = String(".") + AUDIO_EXTENSIONS[i];
        if (lowerName.endsWith(suffix)) return true;
    }
    return false;
}
}  // namespace

bool storageBegin() {
    sdReady = false;

    pinMode(PIN_SD_CS, OUTPUT);
    digitalWrite(PIN_SD_CS, HIGH);

    SPI.begin(PIN_SD_SCK, PIN_SD_MISO, PIN_SD_MOSI, PIN_SD_CS);

    if (!SD.begin(PIN_SD_CS, SPI, SD_SPI_HZ)) {
        Serial.println("[sd] mount failed at 10 MHz, retrying at 4 MHz");
        if (!SD.begin(PIN_SD_CS, SPI, 4000000)) {
            Serial.println("[sd] mount failed");
            return false;
        }
    }

    if (SD.cardType() == CARD_NONE) {
        Serial.println("[sd] no card");
        return false;
    }

    SD.mkdir(AUDIO_DIR);
    SD.mkdir(CONFIG_DIR);
    SD.remove(UPLOAD_TMP_FILE);

    sdReady = true;
    Serial.printf("[sd] mounted, total=%llu used=%llu\n",
                  static_cast<unsigned long long>(SD.totalBytes()),
                  static_cast<unsigned long long>(SD.usedBytes()));
    return true;
}

bool storageReady() {
    return sdReady;
}

uint64_t storageTotalBytes() {
    return sdReady ? SD.totalBytes() : 0;
}

uint64_t storageFreeBytes() {
    if (!sdReady) return 0;
    const uint64_t total = SD.totalBytes();
    const uint64_t used = SD.usedBytes();
    return total > used ? total - used : 0;
}

bool storageIsAudioName(const String& name) {
    String lower = name;
    lower.toLowerCase();
    return hasAudioExtension(lower);
}

String storageSanitizeName(const String& raw) {
    String name = raw;
    name.trim();
    if (name.length() == 0) return "";

    const int slash = name.lastIndexOf('/');
    const int backslash = name.lastIndexOf('\\');
    const int cut = slash > backslash ? slash : backslash;
    if (cut >= 0) name = name.substring(cut + 1);
    name.trim();
    if (name.length() == 0 || name == "." || name == "..") return "";

    const int dot = name.lastIndexOf('.');
    if (dot <= 0 || dot == static_cast<int>(name.length()) - 1) return "";

    String base = name.substring(0, dot);
    String ext = name.substring(dot + 1);
    ext.toLowerCase();
    if (!hasAudioExtension(String(".") + ext)) return "";

    String safe;
    safe.reserve(64);
    for (size_t i = 0; i < base.length(); ++i) {
        const unsigned char c = static_cast<unsigned char>(base[i]);
        if (std::isalnum(c) || c == ' ' || c == '_' || c == '-') {
            safe += static_cast<char>(c);
        } else if (c == '.') {
            safe += '_';
        } else {
            safe += '_';
        }
        if (safe.length() >= 48) break;
    }
    safe.trim();
    if (safe.length() == 0 || safe == "." || safe == "..") safe = "track";

    String result = safe + "." + ext;
    if (result.length() > 80) result = result.substring(0, 80);
    return result;
}

String storageUniqueName(const String& sanitizedName) {
    if (sanitizedName.length() == 0) return "";
    if (!SD.exists(pathFor(sanitizedName))) return sanitizedName;

    const int dot = sanitizedName.lastIndexOf('.');
    String base = sanitizedName.substring(0, dot);
    String ext = sanitizedName.substring(dot);

    for (int i = 1; i < 100; ++i) {
        String candidate = base + "-" + String(i) + ext;
        if (!SD.exists(pathFor(candidate))) return candidate;
    }
    return "";
}

String storagePath(const String& name) {
    return pathFor(name);
}

String storageTempPath() {
    return UPLOAD_TMP_FILE;
}

bool storageFileExists(const String& name) {
    if (!sdReady || name.length() == 0) return false;
    return SD.exists(pathFor(name));
}

bool storageDeleteFile(const String& name) {
    if (!sdReady || name.length() == 0) return false;
    return SD.remove(pathFor(name));
}

bool storageCommit(const String& tempPath, const String& finalName) {
    if (!sdReady || finalName.length() == 0) return false;
    return SD.rename(tempPath, pathFor(finalName));
}

void storageCleanupTemp() {
    if (sdReady) SD.remove(UPLOAD_TMP_FILE);
}

std::vector<AudioFileInfo> storageListAudio() {
    std::vector<AudioFileInfo> files;
    if (!sdReady) return files;

    File dir = SD.open(AUDIO_DIR);
    if (!dir || !dir.isDirectory()) {
        Serial.println("[sd] cannot open audio directory");
        return files;
    }

    File entry = dir.openNextFile();
    while (entry) {
        if (!entry.isDirectory()) {
            String name = entry.name();
            const int slash = name.lastIndexOf('/');
            if (slash >= 0) name = name.substring(slash + 1);

            if (name.length() > 0 && name != ".upload.tmp" && storageIsAudioName(name)) {
                AudioFileInfo info;
                info.name = name;
                info.size = entry.size();
                files.push_back(info);
                if (files.size() >= 500) {
                    entry.close();
                    break;
                }
            }
        }
        entry.close();
        entry = dir.openNextFile();
    }
    dir.close();

    std::sort(files.begin(), files.end(),
              [](const AudioFileInfo& a, const AudioFileInfo& b) {
                  return a.name < b.name;
              });
    return files;
}
