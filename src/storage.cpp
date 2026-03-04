#include "storage.h"
#include "config.h"
#include <LittleFS.h>
#include <Arduino.h>

bool storageInit() {
    if (!LittleFS.begin(true)) {  // true = format on fail
        Serial.println("[Storage] LittleFS mount failed");
        return false;
    }
    Serial.println("[Storage] LittleFS mounted");
    return true;
}

JsonDocument storageReadJson(const char* path) {
    JsonDocument doc;

    File f = LittleFS.open(path, "r");
    if (!f) {
        Serial.printf("[Storage] File not found: %s\n", path);
        return doc;
    }

    DeserializationError err = deserializeJson(doc, f);
    f.close();

    if (err) {
        Serial.printf("[Storage] JSON parse error (%s): %s\n", path, err.c_str());
        doc.clear();
    }
    return doc;
}

bool storageWriteJson(const char* path, const JsonDocument& doc) {
    File f = LittleFS.open(path, "w");
    if (!f) {
        Serial.printf("[Storage] Cannot open for write: %s\n", path);
        return false;
    }

    size_t written = serializeJson(doc, f);
    f.close();

    if (written == 0) {
        Serial.printf("[Storage] Write produced 0 bytes: %s\n", path);
        return false;
    }
    Serial.printf("[Storage] Wrote %u bytes to %s\n", written, path);
    return true;
}
