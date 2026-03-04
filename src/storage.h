#pragma once
#include <ArduinoJson.h>

bool storageInit();

// Returns parsed JSON document; caller must check doc.isNull() on failure.
DynamicJsonDocument storageReadJson(const char* path, size_t capacity = 4096);

bool storageWriteJson(const char* path, const JsonDocument& doc);
