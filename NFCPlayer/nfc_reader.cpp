#include "nfc_reader.h"

#include <cstdio>

#include <Adafruit_PN532.h>
#include <Wire.h>

#include "config.h"

namespace {
Adafruit_PN532 nfc(PIN_NFC_IRQ, PIN_NFC_RST, &Wire);

bool ready = false;
uint32_t lastInitAttempt = 0;

void (*cardHandler)(const String& uid) = nullptr;

String lastUid;
uint32_t lastEventMs = 0;
uint32_t lastSeenMs = 0;
bool present = false;

bool learnActive = false;
String learnUid;
uint32_t learnStartedMs = 0;

String uidToHex(const uint8_t* uid, uint8_t len) {
    String out;
    out.reserve(14);
    char buf[3];
    for (uint8_t i = 0; i < len; ++i) {
        snprintf(buf, sizeof(buf), "%02X", uid[i]);
        out += buf;
    }
    return out;
}

void triggerCard(const String& uid) {
    if (!cardHandler) return;
    cardHandler(uid);
}
}  // namespace

void nfcBegin() {
    lastInitAttempt = millis();
    ready = false;

    Wire.begin(PIN_NFC_SDA, PIN_NFC_SCL);
    Wire.setClock(100000);

    pinMode(PIN_NFC_IRQ, INPUT_PULLUP);
    pinMode(PIN_NFC_RST, OUTPUT);
    digitalWrite(PIN_NFC_RST, HIGH);
    delay(10);
    digitalWrite(PIN_NFC_RST, LOW);
    delay(10);
    digitalWrite(PIN_NFC_RST, HIGH);
    delay(100);

    if (!nfc.begin()) {
        Serial.println("[nfc] PN532 begin failed");
        return;
    }

    const uint32_t version = nfc.getFirmwareVersion();
    if (!version) {
        Serial.println("[nfc] no firmware response (check I2C mode/address 0x24)");
        return;
    }

    nfc.SAMConfig();
    nfc.setPassiveActivationRetries(1);
    ready = true;
    Serial.printf("[nfc] PN532 ready, firmware=0x%08lX\n", static_cast<unsigned long>(version));
}

bool nfcReady() {
    return ready;
}

void nfcSetCardHandler(void (*handler)(const String& uid)) {
    cardHandler = handler;
}

void nfcLearnStart() {
    learnActive = true;
    learnUid = "";
    learnStartedMs = millis();
    lastUid = "";
    present = false;
}

void nfcLearnCancel() {
    learnActive = false;
    learnUid = "";
}

bool nfcLearnActive() {
    if (!learnActive) return false;
    if (millis() - learnStartedMs >= LEARN_TIMEOUT_MS) {
        learnActive = false;
        learnUid = "";
        return false;
    }
    return true;
}

String nfcLearnUid() {
    return learnUid;
}

void nfcLoop() {
    if (!ready) {
        if (millis() - lastInitAttempt >= NFC_REINIT_MS) {
            nfcBegin();
        }
        return;
    }

    if (learnActive && (millis() - learnStartedMs) >= LEARN_TIMEOUT_MS) {
        learnActive = false;
        learnUid = "";
        Serial.println("[nfc] learn window timed out");
    }

    uint8_t uid[7] = {0};
    uint8_t uidLength = 0;
    const bool found = nfc.readPassiveTargetID(
        PN532_MIFARE_ISO14443A, uid, &uidLength, NFC_POLL_TIMEOUT_MS);

    const uint32_t now = millis();

    if (!found) {
        if (present && (now - lastSeenMs) > NFC_ABSENT_MS) {
            present = false;
        }
        return;
    }

    const String uidString = uidToHex(uid, uidLength);
    if (uidString.length() == 0) return;

    if (learnActive) {
        if (learnUid.length() == 0) {
            learnUid = uidString;
            Serial.printf("[nfc] learn captured %s\n", learnUid.c_str());
        }
        present = true;
        lastUid = uidString;
        lastSeenMs = now;
        return;
    }

    lastSeenMs = now;

    if (uidString != lastUid) {
        lastUid = uidString;
        present = true;
        lastEventMs = now;
        triggerCard(uidString);
        return;
    }

    if (!present) {
        if ((now - lastEventMs) >= NFC_COOLDOWN_MS) {
            present = true;
            lastEventMs = now;
            triggerCard(uidString);
        }
        return;
    }

    present = true;
}
