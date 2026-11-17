#include "settings.h"

#include <Preferences.h>

#include "config.h"

namespace {
Settings cfg;
Preferences prefs;

bool beginPrefs(bool readOnly) {
    return prefs.begin("nfcplayer", readOnly);
}
}  // namespace

Settings& settings() {
    return cfg;
}

String settingsSanitizeDeviceName(const String& raw) {
    String out;
    out.reserve(24);
    for (size_t i = 0; i < raw.length(); ++i) {
        char c = raw[i];
        if (c >= 'A' && c <= 'Z') {
            out += char(c - 'A' + 'a');
        } else if ((c >= 'a' && c <= 'z') || (c >= '0' && c <= '9')) {
            out += c;
        } else if (c == '-' || c == '_' || c == ' ') {
            out += '-';
        }
        if (out.length() >= 24) break;
    }
    while (out.length() && out[0] == '-') out.remove(0, 1);
    while (out.length() && out[out.length() - 1] == '-') out.remove(out.length() - 1);
    if (out.length() == 0) out = DEFAULT_DEVICE_NAME;
    return out;
}

void settingsBegin() {
    cfg = Settings();
    cfg.deviceName = DEFAULT_DEVICE_NAME;
    cfg.apPass = DEFAULT_AP_PASS;
    cfg.volume = DEFAULT_VOLUME;

    if (beginPrefs(true)) {
        cfg.wifiSsid = prefs.getString("ssid", "");
        cfg.wifiPass = prefs.getString("pass", "");
        cfg.apPass = prefs.getString("ap_pass", DEFAULT_AP_PASS);
        cfg.webPass = prefs.getString("web_pass", "");
        cfg.deviceName = prefs.getString("dev_name", DEFAULT_DEVICE_NAME);
        cfg.volume = prefs.getUChar("volume", DEFAULT_VOLUME);
        prefs.end();
    }

    if (cfg.apPass.length() < 8) cfg.apPass = DEFAULT_AP_PASS;
    if (cfg.volume > MAX_VOLUME) cfg.volume = MAX_VOLUME;
    cfg.deviceName = settingsSanitizeDeviceName(cfg.deviceName);
}

bool settingsSave() {
    if (!beginPrefs(false)) return false;

    prefs.putString("ssid", cfg.wifiSsid);
    prefs.putString("pass", cfg.wifiPass);
    prefs.putString("ap_pass", cfg.apPass);
    prefs.putString("web_pass", cfg.webPass);
    prefs.putString("dev_name", cfg.deviceName);
    prefs.putUChar("volume", cfg.volume);
    prefs.end();
    return true;
}

void settingsSetWifi(const String& ssid, const String& pass) {
    cfg.wifiSsid = ssid;
    cfg.wifiPass = pass;
    settingsSave();
}

void settingsClearWifi() {
    cfg.wifiSsid = "";
    cfg.wifiPass = "";
    settingsSave();
}

void settingsSetVolume(uint8_t volume) {
    if (volume > MAX_VOLUME) volume = MAX_VOLUME;
    cfg.volume = volume;
    settingsSave();
}
