#pragma once

#include <Arduino.h>

// ---------------------------------------------------------------------------
// Identity
// ---------------------------------------------------------------------------
#define FW_NAME "NFC Player"
#define FW_VERSION "1.0.0"

// ---------------------------------------------------------------------------
// Pin map (ESP32-S3 DevKitC-1, N16R8)
//
// Reserved and NOT used:
//   GPIO0, GPIO3, GPIO45, GPIO46  strapping
//   GPIO19, GPIO20                native USB
//   GPIO26..GPIO37                SPI flash / octal PSRAM
//   GPIO43, GPIO44                UART0
//   GPIO48                        onboard RGB LED on many dev boards
// ---------------------------------------------------------------------------
constexpr uint8_t PIN_I2S_BCLK = 4;
constexpr uint8_t PIN_I2S_LRC = 5;
constexpr uint8_t PIN_I2S_DOUT = 6;

constexpr uint8_t PIN_SD_SCK = 12;
constexpr uint8_t PIN_SD_MISO = 13;
constexpr uint8_t PIN_SD_MOSI = 11;
constexpr uint8_t PIN_SD_CS = 10;
constexpr uint32_t SD_SPI_HZ = 10000000;

constexpr uint8_t PIN_NFC_SDA = 8;
constexpr uint8_t PIN_NFC_SCL = 9;
constexpr uint8_t PIN_NFC_IRQ = 21;
constexpr uint8_t PIN_NFC_RST = 47;

constexpr uint8_t PIN_BTN_STOP = 1;
constexpr uint8_t PIN_BTN_SPARE = 2;
constexpr uint8_t PIN_LED = 7;
constexpr bool LED_ACTIVE_HIGH = true;

// ---------------------------------------------------------------------------
// Storage layout
// ---------------------------------------------------------------------------
constexpr const char* AUDIO_DIR = "/audio";
constexpr const char* CONFIG_DIR = "/config";
constexpr const char* CARDS_FILE = "/config/cards.json";
constexpr const char* CARDS_TMP_FILE = "/config/cards.tmp";
constexpr const char* UPLOAD_TMP_FILE = "/audio/.upload.tmp";

// ---------------------------------------------------------------------------
// Limits and timing
// ---------------------------------------------------------------------------
constexpr size_t MAX_UPLOAD_BYTES = 64UL * 1024UL * 1024UL;  // 64 MB
constexpr size_t MIN_FREE_BYTES = 2UL * 1024UL * 1024UL;     // keep 2 MB free
constexpr size_t UPLOAD_CHUNK_BYTES = 1436;                  // WebServer HTTPUpload buf

constexpr uint32_t NFC_COOLDOWN_MS = 2000;
constexpr uint32_t NFC_ABSENT_MS = 1500;
constexpr uint32_t NFC_POLL_TIMEOUT_MS = 20;
constexpr uint32_t NFC_REINIT_MS = 5000;
constexpr uint32_t LEARN_TIMEOUT_MS = 60000;

constexpr uint32_t BUTTON_DEBOUNCE_MS = 30;
constexpr uint32_t BUTTON_LONG_MS = 3000;

constexpr uint8_t DEFAULT_VOLUME = 15;
constexpr uint8_t MAX_VOLUME = 21;
constexpr uint8_t VOLUME_STEPS = 21;

// ---------------------------------------------------------------------------
// Network defaults
// ---------------------------------------------------------------------------
constexpr const char* AP_SSID_PREFIX = "NFCPlayer-";
constexpr const char* DEFAULT_AP_PASS = "nfcplayer";
constexpr const char* DEFAULT_DEVICE_NAME = "nfcplayer";
constexpr uint32_t STA_CONNECT_TIMEOUT_MS = 15000;
constexpr uint32_t STA_RETRY_MS = 30000;
constexpr uint32_t AP_FALLBACK_AFTER_MS = 60000;

// ---------------------------------------------------------------------------
// Audio formats accepted for upload (the decoder must support them)
// ---------------------------------------------------------------------------
constexpr const char* AUDIO_EXTENSIONS[] = {
    "mp3", "m4a", "aac", "flac", "ogg", "opus", "wav",
};
constexpr size_t AUDIO_EXTENSION_COUNT =
    sizeof(AUDIO_EXTENSIONS) / sizeof(AUDIO_EXTENSIONS[0]);
