#include "cards.h"

#include <ArduinoJson.h>
#include <SD.h>

#include <cctype>

#include "config.h"
#include "storage.h"

namespace {
std::vector<CardMapping> mappings;

int findIndex(const String& uid) {
    for (size_t i = 0; i < mappings.size(); ++i) {
        if (mappings[i].uid == uid) return static_cast<int>(i);
    }
    return -1;
}
}  // namespace

String cardsNormalizeUid(const String& raw) {
    String out;
    out.reserve(14);
    for (size_t i = 0; i < raw.length(); ++i) {
        const unsigned char c = static_cast<unsigned char>(raw[i]);
        if (std::isxdigit(c)) {
            out += static_cast<char>(std::toupper(c));
        } else if (c == ':' || c == '-' || c == ' ') {
            continue;
        } else {
            return "";
        }
    }
    if (out.length() != 8 && out.length() != 14) return "";
    return out;
}

void cardsBegin() {
    mappings.clear();
    if (!storageReady()) return;

    File f = SD.open(CARDS_FILE, FILE_READ);
    if (!f) {
        Serial.println("[cards] no cards.json yet");
        return;
    }

    JsonDocument doc;
    const DeserializationError err = deserializeJson(doc, f);
    f.close();
    if (err) {
        Serial.printf("[cards] parse error: %s\n", err.c_str());
        return;
    }

    JsonArray arr = doc["cards"].as<JsonArray>();
    for (JsonObject o : arr) {
        const String uid = cardsNormalizeUid(o["uid"].as<String>());
        String file = o["file"].as<String>();
        const String label = o["label"].as<String>();
        if (uid.length() == 0) continue;
        if (!storageIsAudioName(file)) continue;
        const int slash = file.lastIndexOf('/');
        const int backslash = file.lastIndexOf('\\');
        const int cut = slash > backslash ? slash : backslash;
        if (cut >= 0) file = file.substring(cut + 1);
        if (file.length() == 0 || file.indexOf("..") >= 0) continue;
        if (findIndex(uid) >= 0) continue;

        CardMapping m;
        m.uid = uid;
        m.file = file;
        m.label = label.length() ? label : file;
        mappings.push_back(m);
    }
    Serial.printf("[cards] loaded %u mappings\n", static_cast<unsigned>(mappings.size()));
}

bool cardsSave() {
    if (!storageReady()) return false;

    JsonDocument doc;
    JsonArray arr = doc["cards"].to<JsonArray>();
    for (const CardMapping& m : mappings) {
        JsonObject o = arr.add<JsonObject>();
        o["uid"] = m.uid;
        o["file"] = m.file;
        o["label"] = m.label;
    }

    File f = SD.open(CARDS_TMP_FILE, FILE_WRITE);
    if (!f) {
        Serial.println("[cards] cannot write temp file");
        return false;
    }
    serializeJson(doc, f);
    f.flush();
    f.close();

    SD.remove(CARDS_FILE);
    const bool ok = SD.rename(CARDS_TMP_FILE, CARDS_FILE);
    if (!ok) {
        Serial.println("[cards] rename failed");
        SD.remove(CARDS_TMP_FILE);
    }
    return ok;
}

std::vector<CardMapping> cardsAll() {
    return mappings;
}

const CardMapping* cardsFind(const String& uid) {
    const int idx = findIndex(uid);
    return idx >= 0 ? &mappings[static_cast<size_t>(idx)] : nullptr;
}

bool cardsUpsert(const String& uidRaw, const String& file, const String& label) {
    const String uid = cardsNormalizeUid(uidRaw);
    if (uid.length() == 0 || !storageIsAudioName(file)) return false;

    const int idx = findIndex(uid);
    if (idx >= 0) {
        mappings[static_cast<size_t>(idx)].file = file;
        mappings[static_cast<size_t>(idx)].label = label.length() ? label : file;
        return true;
    }

    CardMapping m;
    m.uid = uid;
    m.file = file;
    m.label = label.length() ? label : file;
    mappings.push_back(m);
    return true;
}

bool cardsRemove(const String& uidRaw) {
    const String uid = cardsNormalizeUid(uidRaw);
    if (uid.length() == 0) return false;

    for (size_t i = 0; i < mappings.size(); ++i) {
        if (mappings[i].uid == uid) {
            mappings.erase(mappings.begin() + static_cast<long>(i));
            return true;
        }
    }
    return false;
}

bool cardsRemoveByFile(const String& file) {
    bool removed = false;
    for (size_t i = 0; i < mappings.size();) {
        if (mappings[i].file == file) {
            mappings.erase(mappings.begin() + static_cast<long>(i));
            removed = true;
        } else {
            ++i;
        }
    }
    return removed;
}
