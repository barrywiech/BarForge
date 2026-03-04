#pragma once
#include <ArduinoJson.h>

bool storageInit();

// Returns parsed JsonDocument; check doc.isNull() on failure.
JsonDocument storageReadJson(const char* path);

bool storageWriteJson(const char* path, const JsonDocument& doc);
