#include "net.h"

#include <cstdio>

#include <ESPmDNS.h>
#include <WiFi.h>

#include "config.h"
#include "settings.h"

namespace {
bool apActive = false;
bool mdnsStarted = false;
String apSsid;
uint32_t lastStaAttempt = 0;
uint32_t disconnectedSince = 0;

void startMdns() {
    if (mdnsStarted) {
        MDNS.end();
        mdnsStarted = false;
    }
    mdnsStarted = MDNS.begin(settings().deviceName.c_str());
    if (mdnsStarted) {
        MDNS.addService("http", "tcp", 80);
        Serial.printf("[net] mDNS: http://%s.local\n", settings().deviceName.c_str());
    } else {
        Serial.println("[net] mDNS start failed");
    }
}

void startAp() {
    if (apActive) return;

    WiFi.mode(WIFI_AP);
    const bool ok = WiFi.softAP(apSsid.c_str(), settings().apPass.c_str());
    apActive = true;
    Serial.printf("[net] AP %s -> %s (%s)\n", apSsid.c_str(),
                  WiFi.softAPIP().toString().c_str(), ok ? "ok" : "failed");
    startMdns();
}
}  // namespace

void netBegin() {
    WiFi.persistent(false);
    WiFi.setSleep(false);

    const uint32_t macLow = static_cast<uint32_t>(ESP.getEfuseMac() & 0xFFFFULL);
    char suffix[8];
    snprintf(suffix, sizeof(suffix), "%04X", macLow);
    apSsid = String(AP_SSID_PREFIX) + suffix;

    if (settings().wifiSsid.length() > 0) {
        WiFi.mode(WIFI_STA);
        WiFi.setHostname(settings().deviceName.c_str());
        WiFi.begin(settings().wifiSsid.c_str(), settings().wifiPass.c_str());

        const uint32_t started = millis();
        while (WiFi.status() != WL_CONNECTED &&
               (millis() - started) < STA_CONNECT_TIMEOUT_MS) {
            delay(100);
        }

        if (WiFi.status() == WL_CONNECTED) {
            Serial.printf("[net] joined %s -> %s\n",
                          settings().wifiSsid.c_str(),
                          WiFi.localIP().toString().c_str());
            startMdns();
            return;
        }

        Serial.printf("[net] could not join %s\n", settings().wifiSsid.c_str());
    }

    startAp();
}

void netLoop() {
    if (apActive) return;

    if (settings().wifiSsid.length() == 0) {
        startAp();
        return;
    }

    if (WiFi.status() == WL_CONNECTED) {
        disconnectedSince = 0;
        return;
    }

    const uint32_t now = millis();
    if (disconnectedSince == 0) {
        disconnectedSince = now;
        Serial.println("[net] Wi-Fi disconnected; will retry/fallback");
    }

    if ((now - disconnectedSince) >= AP_FALLBACK_AFTER_MS) {
        startAp();
        return;
    }

    if ((now - lastStaAttempt) >= STA_RETRY_MS) {
        lastStaAttempt = now;
        WiFi.disconnect();
        WiFi.begin(settings().wifiSsid.c_str(), settings().wifiPass.c_str());
    }
}

bool netStaConnected() {
    return WiFi.status() == WL_CONNECTED;
}

bool netApActive() {
    return apActive;
}

String netIp() {
    if (apActive) return WiFi.softAPIP().toString();
    return WiFi.localIP().toString();
}

String netApSsid() {
    return apSsid;
}

int netRssi() {
    if (apActive) return 0;
    return WiFi.RSSI();
}

bool netSaveWifi(const String& ssid, const String& pass) {
    settingsSetWifi(ssid, pass);
    return true;
}

void netReboot() {
    Serial.println("[net] restarting");
    delay(100);
    ESP.restart();
}
