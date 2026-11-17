#include "web_server.h"

#include <ArduinoJson.h>
#include <SD.h>
#include <WebServer.h>
#include <WiFi.h>

#include "audio_player.h"
#include "cards.h"
#include "config.h"
#include "net.h"
#include "nfc_reader.h"
#include "player.h"
#include "settings.h"
#include "storage.h"
#include "web_ui.h"

namespace {
WebServer server(80);

File uploadFile;
bool uploadActive = false;
bool uploadAuthFailed = false;
bool uploadTooLarge = false;
String uploadName;
String uploadError;
size_t uploadReceived = 0;

bool isAuthorized() {
    if (settings().webPass.length() == 0) return true;
    return server.authenticate("admin", settings().webPass.c_str());
}

bool requireAuth() {
    if (isAuthorized()) return true;
    server.requestAuthentication(BASIC_AUTH, "NFC Player");
    return false;
}

void sendJson(int code, JsonDocument& doc) {
    String out;
    serializeJson(doc, out);
    server.send(code, "application/json", out);
}

void sendError(int code, const String& message) {
    JsonDocument doc;
    doc["error"] = message;
    sendJson(code, doc);
}

String stateName(PlayerState state) {
    switch (state) {
        case PlayerState::PLAYING:
            return "playing";
        case PlayerState::LEARNING:
            return "learning";
        default:
            return "idle";
    }
}

// Accept a name that already exists on the SD card, but never a path.
String existingAudioName(const String& raw) {
    String name = raw;
    name.trim();
    if (name.length() == 0) return "";
    if (name.indexOf('/') >= 0 || name.indexOf('\\') >= 0) return "";
    if (name.indexOf("..") >= 0) return "";
    if (!storageIsAudioName(name)) return "";
    return name;
}

void handleRoot() {
    if (!requireAuth()) return;
    server.send_P(200, "text/html", INDEX_HTML);
}

void handleStatus() {
    if (!requireAuth()) return;

    JsonDocument doc;
    doc["state"] = stateName(playerState());
    doc["file"] = playerCurrentFile();
    doc["card"] = playerCurrentUid();
    doc["volume"] = audioGetVolume();
    doc["position"] = audioElapsedSeconds();
    doc["duration"] = audioDurationSeconds();
    doc["ip"] = netIp();
    doc["ap"] = netApActive();
    doc["ap_ssid"] = netApSsid();
    doc["rssi"] = netRssi();
    doc["nfc"] = nfcReady();
    doc["sd"] = storageReady();
    doc["free"] = static_cast<double>(storageFreeBytes());
    doc["total"] = static_cast<double>(storageTotalBytes());
    doc["uptime"] = millis() / 1000;
    doc["version"] = FW_VERSION;
    doc["learn"] = playerLearnActive();
    sendJson(200, doc);
}

void handleFiles() {
    if (!requireAuth()) return;

    JsonDocument doc;
    JsonArray arr = doc["files"].to<JsonArray>();
    for (const AudioFileInfo& f : storageListAudio()) {
        JsonObject o = arr.add<JsonObject>();
        o["name"] = f.name;
        o["size"] = f.size;
    }
    doc["free"] = static_cast<double>(storageFreeBytes());
    doc["total"] = static_cast<double>(storageTotalBytes());
    sendJson(200, doc);
}

void handleCards() {
    if (!requireAuth()) return;

    JsonDocument doc;
    JsonArray arr = doc["cards"].to<JsonArray>();
    for (const CardMapping& c : cardsAll()) {
        JsonObject o = arr.add<JsonObject>();
        o["uid"] = c.uid;
        o["file"] = c.file;
        o["label"] = c.label;
        o["exists"] = storageFileExists(c.file);
    }
    sendJson(200, doc);
}

void handlePlay() {
    if (!requireAuth()) return;

    const String name = existingAudioName(server.arg("name"));
    if (name.length() == 0) {
        sendError(400, "invalid file name");
        return;
    }
    if (!storageFileExists(name)) {
        sendError(404, "file not found");
        return;
    }
    if (!playerPlayFile(name)) {
        sendError(500, "playback failed");
        return;
    }

    JsonDocument doc;
    doc["ok"] = true;
    doc["file"] = name;
    sendJson(200, doc);
}

void handleDelete() {
    if (!requireAuth()) return;

    const String name = existingAudioName(server.arg("name"));
    if (name.length() == 0 || !storageFileExists(name)) {
        sendError(404, "file not found");
        return;
    }

    playerStop();
    delay(100);
    cardsRemoveByFile(name);
    cardsSave();

    if (!storageDeleteFile(name)) {
        sendError(500, "could not delete file");
        return;
    }

    JsonDocument doc;
    doc["ok"] = true;
    sendJson(200, doc);
}

void handleStop() {
    if (!requireAuth()) return;
    playerStop();
    JsonDocument doc;
    doc["ok"] = true;
    sendJson(200, doc);
}

void handleVolume() {
    if (!requireAuth()) return;
    int level = server.arg("level").toInt();
    if (level < 0) level = 0;
    if (level > MAX_VOLUME) level = MAX_VOLUME;
    audioSetVolume(static_cast<uint8_t>(level));

    JsonDocument doc;
    doc["ok"] = true;
    doc["volume"] = audioGetVolume();
    sendJson(200, doc);
}

void handleLearnStart() {
    if (!requireAuth()) return;
    playerStartLearn();
    JsonDocument doc;
    doc["ok"] = true;
    doc["active"] = playerLearnActive();
    sendJson(200, doc);
}

void handleLearnStatus() {
    if (!requireAuth()) return;
    JsonDocument doc;
    doc["active"] = playerLearnActive();
    doc["uid"] = playerLearnUid();
    sendJson(200, doc);
}

void handleLearnCancel() {
    if (!requireAuth()) return;
    playerCancelLearn();
    JsonDocument doc;
    doc["ok"] = true;
    sendJson(200, doc);
}

void handleCardsSet() {
    if (!requireAuth()) return;

    const String uid = cardsNormalizeUid(server.arg("uid"));
    const String file = existingAudioName(server.arg("file"));
    const String label = server.arg("label");

    if (uid.length() == 0) {
        sendError(400, "invalid card UID");
        return;
    }
    if (file.length() == 0 || !storageFileExists(file)) {
        sendError(400, "choose an existing audio file");
        return;
    }

    playerStop();
    delay(100);
    if (!cardsUpsert(uid, file, label)) {
        sendError(500, "could not save card");
        return;
    }
    if (!cardsSave()) {
        sendError(500, "could not write cards.json");
        return;
    }

    playerCancelLearn();

    JsonDocument doc;
    doc["ok"] = true;
    doc["uid"] = uid;
    doc["file"] = file;
    sendJson(200, doc);
}

void handleCardsDelete() {
    if (!requireAuth()) return;

    const String uid = cardsNormalizeUid(server.arg("uid"));
    if (uid.length() == 0 || !cardsRemove(uid)) {
        sendError(404, "card not found");
        return;
    }
    playerStop();
    delay(100);
    cardsSave();

    JsonDocument doc;
    doc["ok"] = true;
    sendJson(200, doc);
}

void handleSettingsGet() {
    if (!requireAuth()) return;

    JsonDocument doc;
    doc["device"] = settings().deviceName;
    doc["volume"] = settings().volume;
    doc["ap_pass"] = settings().apPass;
    doc["ap"] = netApActive();
    doc["ap_ssid"] = netApSsid();
    doc["ip"] = netIp();
    doc["ssid"] = settings().wifiSsid;
    doc["web_auth"] = settings().webPass.length() > 0;
    doc["fw"] = FW_VERSION;
    doc["free"] = static_cast<double>(storageFreeBytes());
    doc["total"] = static_cast<double>(storageTotalBytes());
    doc["nfc"] = nfcReady();
    doc["sd"] = storageReady();
    sendJson(200, doc);
}

void handleSettingsSet() {
    if (!requireAuth()) return;

    bool rebootRequired = false;
    String message = "saved";

    if (server.hasArg("device")) {
        const String device = settingsSanitizeDeviceName(server.arg("device"));
        if (device != settings().deviceName) {
            settings().deviceName = device;
            rebootRequired = true;
        }
    }

    if (server.hasArg("ap_pass")) {
        const String apPass = server.arg("ap_pass");
        if (apPass.length() < 8) {
            sendError(400, "AP password must be at least 8 characters");
            return;
        }
        if (apPass != settings().apPass) {
            settings().apPass = apPass;
            rebootRequired = true;
        }
    }

    if (server.arg("web_clear") == "1") {
        settings().webPass = "";
    } else if (server.hasArg("web_pass") && server.arg("web_pass").length() > 0) {
        settings().webPass = server.arg("web_pass");
    }

    if (server.hasArg("volume")) {
        int level = server.arg("volume").toInt();
        if (level < 0) level = 0;
        if (level > MAX_VOLUME) level = MAX_VOLUME;
        audioSetVolume(static_cast<uint8_t>(level));
    }

    settingsSave();

    if (rebootRequired) {
        message = "saved; reboot to apply the device name or AP password";
    }

    JsonDocument doc;
    doc["ok"] = true;
    doc["message"] = message;
    doc["reboot_required"] = rebootRequired;
    sendJson(200, doc);
}

void handleWifiSave() {
    if (!requireAuth()) return;

    const String ssid = server.arg("ssid");
    const String pass = server.arg("pass");

    if (ssid.length() == 0) {
        settingsClearWifi();
    } else {
        if (pass.length() > 0 && pass.length() < 8) {
            sendError(400, "Wi-Fi password must be empty or at least 8 characters");
            return;
        }
        settingsSetWifi(ssid, pass);
    }

    JsonDocument doc;
    doc["ok"] = true;
    doc["message"] = "saved; rebooting";
    sendJson(200, doc);

    delay(500);
    netReboot();
}

void handleWifiScan() {
    if (!requireAuth()) return;

    const int count = WiFi.scanNetworks();
    JsonDocument doc;
    JsonArray arr = doc["networks"].to<JsonArray>();

    if (count > 0) {
        for (int i = 0; i < count; ++i) {
            JsonObject o = arr.add<JsonObject>();
            o["ssid"] = WiFi.SSID(i);
            o["rssi"] = WiFi.RSSI(i);
            o["secure"] = WiFi.encryptionType(i) != WIFI_AUTH_OPEN;
        }
    }
    WiFi.scanDelete();
    doc["count"] = count > 0 ? count : 0;
    sendJson(200, doc);
}

void handleReboot() {
    if (!requireAuth()) return;

    JsonDocument doc;
    doc["ok"] = true;
    doc["message"] = "rebooting";
    sendJson(200, doc);

    delay(300);
    netReboot();
}

void handleNotFound() {
    sendError(404, "not found");
}

void handleUploadChunk() {
    HTTPUpload& up = server.upload();

    if (up.status == UPLOAD_FILE_START) {
        uploadActive = false;
        uploadAuthFailed = false;
        uploadTooLarge = false;
        uploadName = "";
        uploadError = "";
        uploadReceived = 0;

        if (!isAuthorized()) {
            uploadAuthFailed = true;
            return;
        }

        if (!storageReady()) {
            uploadError = "SD card not ready";
            return;
        }

        const String safe = storageSanitizeName(up.filename);
        if (safe.length() == 0) {
            uploadError = "unsupported audio file type";
            return;
        }

        const String unique = storageUniqueName(safe);
        if (unique.length() == 0) {
            uploadError = "too many files with that name";
            return;
        }

        if (up.totalSize > MAX_UPLOAD_BYTES) {
            uploadTooLarge = true;
            uploadError = "file is larger than 64 MB";
            return;
        }

        if (up.totalSize > 0 &&
            storageFreeBytes() < static_cast<uint64_t>(up.totalSize) + MIN_FREE_BYTES) {
            uploadError = "not enough free space on the SD card";
            return;
        }

        playerStop();
        delay(100);

        uploadFile = SD.open(UPLOAD_TMP_FILE, FILE_WRITE);
        if (!uploadFile) {
            uploadError = "cannot create temporary file";
            return;
        }

        uploadName = unique;
        uploadActive = true;
        Serial.printf("[web] upload start: %s (%u bytes)\n", uploadName.c_str(),
                      static_cast<unsigned>(up.totalSize));
        return;
    }

    if (up.status == UPLOAD_FILE_WRITE) {
        if (!uploadActive || uploadError.length() > 0) return;

        if (uploadReceived + up.currentSize > MAX_UPLOAD_BYTES) {
            uploadTooLarge = true;
            uploadError = "file is larger than 64 MB";
            uploadFile.close();
            SD.remove(UPLOAD_TMP_FILE);
            uploadActive = false;
            return;
        }

        const size_t written = uploadFile.write(up.buf, up.currentSize);
        if (written != up.currentSize) {
            uploadError = "SD write failed";
            uploadFile.close();
            SD.remove(UPLOAD_TMP_FILE);
            uploadActive = false;
            return;
        }
        uploadReceived += written;
        return;
    }

    if (up.status == UPLOAD_FILE_END) {
        if (uploadFile) {
            uploadFile.flush();
            uploadFile.close();
        }

        if (uploadActive && uploadError.length() == 0 && uploadReceived > 0) {
            if (!storageCommit(UPLOAD_TMP_FILE, uploadName)) {
                uploadError = "could not rename uploaded file";
                SD.remove(UPLOAD_TMP_FILE);
            } else {
                Serial.printf("[web] upload saved: %s (%u bytes)\n", uploadName.c_str(),
                              static_cast<unsigned>(uploadReceived));
            }
        } else {
            if (uploadActive && uploadReceived == 0) uploadError = "empty file";
            SD.remove(UPLOAD_TMP_FILE);
        }
        uploadActive = false;
        return;
    }

    if (up.status == UPLOAD_FILE_ABORTED) {
        if (uploadFile) uploadFile.close();
        SD.remove(UPLOAD_TMP_FILE);
        uploadActive = false;
        if (uploadError.length() == 0) uploadError = "upload aborted";
    }
}

void handleUploadDone() {
    // Snapshot and clear the per-request state so a POST without a file
    // cannot accidentally report the previous upload.
    const bool authFailed = uploadAuthFailed;
    const bool tooLarge = uploadTooLarge;
    const String error = uploadError;
    const String name = uploadName;
    const size_t size = uploadReceived;

    uploadAuthFailed = false;
    uploadTooLarge = false;
    uploadError = "";
    uploadName = "";
    uploadReceived = 0;
    uploadActive = false;

    if (authFailed) {
        server.requestAuthentication(BASIC_AUTH, "NFC Player");
        return;
    }

    if (error.length() > 0) {
        sendError(tooLarge ? 413 : 400, error);
        return;
    }

    if (name.length() == 0 || size == 0) {
        sendError(400, "no file received");
        return;
    }

    JsonDocument doc;
    doc["ok"] = true;
    doc["name"] = name;
    doc["size"] = size;
    sendJson(200, doc);
}
}  // namespace

void webBegin() {
    server.on("/", HTTP_GET, handleRoot);
    server.on("/api/status", HTTP_GET, handleStatus);
    server.on("/api/files", HTTP_GET, handleFiles);
    server.on("/api/cards", HTTP_GET, handleCards);
    server.on("/api/play", HTTP_POST, handlePlay);
    server.on("/api/delete", HTTP_POST, handleDelete);
    server.on("/api/stop", HTTP_POST, handleStop);
    server.on("/api/volume", HTTP_POST, handleVolume);
    server.on("/api/learn/start", HTTP_POST, handleLearnStart);
    server.on("/api/learn", HTTP_GET, handleLearnStatus);
    server.on("/api/learn/cancel", HTTP_POST, handleLearnCancel);
    server.on("/api/cards", HTTP_POST, handleCardsSet);
    server.on("/api/cards/delete", HTTP_POST, handleCardsDelete);
    server.on("/api/settings", HTTP_GET, handleSettingsGet);
    server.on("/api/settings", HTTP_POST, handleSettingsSet);
    server.on("/api/wifi", HTTP_POST, handleWifiSave);
    server.on("/api/wifi/scan", HTTP_GET, handleWifiScan);
    server.on("/api/reboot", HTTP_POST, handleReboot);
    server.on("/api/upload", HTTP_POST, handleUploadDone, handleUploadChunk);
    server.onNotFound(handleNotFound);
    server.begin();
    Serial.println("[web] listening on port 80");
}

void webLoop() {
    server.handleClient();
}
