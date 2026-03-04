#include <Arduino.h>
#include <WiFiManager.h>
#include "config.h"
#include "storage.h"
#include "pump_control.h"
#include "display.h"
#include "web_server.h"

static unsigned long lastDisplayUpdate = 0;

void setup() {
    Serial.begin(115200);
    Serial.println("\n[BarForge] Booting...");

    // 1. Storage
    storageInit();

    // 2. Pump control (sets relay GPIOs, attaches ESTOP interrupt)
    pumpsInit();

    // 3. WiFiManager — captive portal on first boot, auto-connect on subsequent
    WiFiManager wm;
    wm.setConfigPortalTimeout(120);  // portal times out after 2 min, then continues
    if (!wm.autoConnect(WIFI_AP_NAME)) {
        Serial.println("[WiFi] Failed to connect — continuing offline");
    } else {
        Serial.printf("[WiFi] Connected. IP: %s\n", WiFi.localIP().toString().c_str());
    }

    // 4. Display (shows IP once WiFi is up)
    displayInit();

    // 5. Web server (also loads pump config from LittleFS)
    webServerInit();

    Serial.println("[BarForge] Ready");
}

void loop() {
    // Non-blocking pump timer check
    pumpsUpdate();

    // Periodic display refresh
    unsigned long now = millis();
    if (now - lastDisplayUpdate >= DISPLAY_UPDATE_MS) {
        lastDisplayUpdate = now;
        displayUpdate();
    }

    // Safety: if ESTOP was triggered via ISR, clear it when button is released
    // GPIO 34 has external pull-up; button pressed = LOW
    if (emergencyStop && digitalRead(ESTOP_PIN) == HIGH) {
        // Button released — allow reset via web API /api/stop only (don't auto-clear here)
        // Keep emergencyStop set until operator acknowledges via web UI
    }
}
