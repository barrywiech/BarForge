#pragma once

// ─── Display (ILI9488 via SPI) ───────────────────────────────────────────────
#define TFT_CS    5
#define TFT_DC   16
#define TFT_RST  17
#define TFT_BL   21
// SCK = GPIO 18, MOSI = GPIO 23, MISO = GPIO 19 (hardware SPI defaults)

// ─── Relay board (active-LOW SSR) ────────────────────────────────────────────
#define PUMP1_PIN  26
#define PUMP2_PIN  27
#define PUMP3_PIN  25
#define PUMP4_PIN  33
#define PUMP5_PIN  32
#define RELAY_SPARE_PIN 22

#define NUM_PUMPS  5

// ─── Emergency stop ───────────────────────────────────────────────────────────
#define ESTOP_PIN  34   // input-only GPIO; external 10kΩ pull-up to 3V3

// ─── LittleFS paths ───────────────────────────────────────────────────────────
#define CONFIG_PATH   "/config.json"
#define RECIPES_PATH  "/recipes.json"

// ─── Calibration ─────────────────────────────────────────────────────────────
#define CALIBRATION_RUN_SECS  5.0f    // pump runs this long during calibration
#define DEFAULT_ML_PER_SEC    2.0f    // fallback until calibrated

// ─── Display update interval ──────────────────────────────────────────────────
#define DISPLAY_UPDATE_MS  500

// ─── WiFi AP name (captive portal on first boot) ─────────────────────────────
#define WIFI_AP_NAME  "BarForge-Setup"
