#include "web_server.h"
#include "config.h"
#include "storage.h"
#include "pump_control.h"
#include "display.h"
#include <ESPAsyncWebServer.h>
#include <LittleFS.h>
#include <ArduinoJson.h>
#include <WiFi.h>

static AsyncWebServer server(80);

// ─── Helpers ──────────────────────────────────────────────────────────────────
static void sendJson(AsyncWebServerRequest* req, const JsonDocument& doc, int code = 200) {
    String body;
    serializeJson(doc, body);
    req->send(code, "application/json", body);
}

static void sendOk(AsyncWebServerRequest* req, const char* msg = "ok") {
    StaticJsonDocument<64> doc;
    doc["status"] = msg;
    sendJson(req, doc);
}

static void sendError(AsyncWebServerRequest* req, const char* msg, int code = 400) {
    StaticJsonDocument<128> doc;
    doc["error"] = msg;
    sendJson(req, doc, code);
}

// ─── Load pump config from JSON into pumpConfigs[] ────────────────────────────
static void loadPumpConfigsFromStorage() {
    DynamicJsonDocument doc = storageReadJson(CONFIG_PATH);
    if (doc.isNull()) return;

    JsonArray pumps = doc["pumps"].as<JsonArray>();
    for (JsonObject p : pumps) {
        int idx = (int)p["id"] - 1;
        if (idx < 0 || idx >= NUM_PUMPS) continue;
        strlcpy(pumpConfigs[idx].spirit, p["spirit"] | "Unknown", 32);
        pumpConfigs[idx].bottleSize = p["bottleSize"] | 700.0f;
        pumpConfigs[idx].remaining  = p["remaining"]  | 700.0f;
        pumpConfigs[idx].mlPerSec   = p["mlPerSec"]   | DEFAULT_ML_PER_SEC;
    }
}

// ─── Save pumpConfigs[] to storage ───────────────────────────────────────────
static void savePumpConfigsToStorage() {
    DynamicJsonDocument doc(2048);
    JsonArray pumps = doc.createNestedArray("pumps");
    for (int i = 0; i < NUM_PUMPS; i++) {
        JsonObject p = pumps.createNestedObject();
        p["id"]         = i + 1;
        p["spirit"]     = pumpConfigs[i].spirit;
        p["bottleSize"] = pumpConfigs[i].bottleSize;
        p["remaining"]  = pumpConfigs[i].remaining;
        p["mlPerSec"]   = pumpConfigs[i].mlPerSec;
    }
    storageWriteJson(CONFIG_PATH, doc);
}

// ─── Routes ───────────────────────────────────────────────────────────────────
void webServerInit() {
    // Load pump configs from LittleFS at startup
    loadPumpConfigsFromStorage();

    // Static files from LittleFS
    server.serveStatic("/", LittleFS, "/").setDefaultFile("index.html");

    // ── GET /api/status ──────────────────────────────────────────────────────
    server.on("/api/status", HTTP_GET, [](AsyncWebServerRequest* req) {
        DynamicJsonDocument doc(1024);
        doc["status"] = getStatusString();
        doc["estop"]  = (bool)emergencyStop;
        doc["ip"]     = WiFi.localIP().toString();

        JsonArray pumps = doc.createNestedArray("pumps");
        for (int i = 0; i < NUM_PUMPS; i++) {
            JsonObject p = pumps.createNestedObject();
            p["id"]        = i + 1;
            p["spirit"]    = pumpConfigs[i].spirit;
            p["bottleSize"]= pumpConfigs[i].bottleSize;
            p["remaining"] = pumpConfigs[i].remaining;
            p["mlPerSec"]  = pumpConfigs[i].mlPerSec;
            p["active"]    = isPumpActive(i);
        }
        sendJson(req, doc);
    });

    // ── GET /api/config ──────────────────────────────────────────────────────
    server.on("/api/config", HTTP_GET, [](AsyncWebServerRequest* req) {
        DynamicJsonDocument doc(2048);
        JsonArray pumps = doc.createNestedArray("pumps");
        for (int i = 0; i < NUM_PUMPS; i++) {
            JsonObject p = pumps.createNestedObject();
            p["id"]         = i + 1;
            p["spirit"]     = pumpConfigs[i].spirit;
            p["bottleSize"] = pumpConfigs[i].bottleSize;
            p["remaining"]  = pumpConfigs[i].remaining;
            p["mlPerSec"]   = pumpConfigs[i].mlPerSec;
        }
        sendJson(req, doc);
    });

    // ── POST /api/config ─────────────────────────────────────────────────────
    server.on("/api/config", HTTP_POST,
        [](AsyncWebServerRequest* req) {},
        nullptr,
        [](AsyncWebServerRequest* req, uint8_t* data, size_t len, size_t index, size_t total) {
            DynamicJsonDocument doc(2048);
            if (deserializeJson(doc, data, len)) {
                sendError(req, "Invalid JSON");
                return;
            }
            JsonArray pumps = doc["pumps"].as<JsonArray>();
            for (JsonObject p : pumps) {
                int idx = (int)p["id"] - 1;
                if (idx < 0 || idx >= NUM_PUMPS) continue;
                if (p.containsKey("spirit"))     strlcpy(pumpConfigs[idx].spirit, p["spirit"], 32);
                if (p.containsKey("bottleSize")) pumpConfigs[idx].bottleSize = p["bottleSize"];
                if (p.containsKey("remaining"))  pumpConfigs[idx].remaining  = p["remaining"];
                if (p.containsKey("mlPerSec"))   pumpConfigs[idx].mlPerSec   = p["mlPerSec"];
            }
            savePumpConfigsToStorage();
            sendOk(req, "config saved");
        }
    );

    // ── GET /api/recipes ─────────────────────────────────────────────────────
    server.on("/api/recipes", HTTP_GET, [](AsyncWebServerRequest* req) {
        DynamicJsonDocument doc = storageReadJson(RECIPES_PATH, 8192);
        if (doc.isNull()) {
            DynamicJsonDocument empty(64);
            empty.createNestedArray("recipes");
            sendJson(req, empty);
        } else {
            sendJson(req, doc);
        }
    });

    // ── POST /api/recipes ────────────────────────────────────────────────────
    server.on("/api/recipes", HTTP_POST,
        [](AsyncWebServerRequest* req) {},
        nullptr,
        [](AsyncWebServerRequest* req, uint8_t* data, size_t len, size_t index, size_t total) {
            DynamicJsonDocument doc(8192);
            if (deserializeJson(doc, data, len)) {
                sendError(req, "Invalid JSON");
                return;
            }
            storageWriteJson(RECIPES_PATH, doc);
            sendOk(req, "recipes saved");
        }
    );

    // ── POST /api/pour/recipe ─────────────────────────────────────────────────
    server.on("/api/pour/recipe", HTTP_POST,
        [](AsyncWebServerRequest* req) {},
        nullptr,
        [](AsyncWebServerRequest* req, uint8_t* data, size_t len, size_t index, size_t total) {
            if (emergencyStop) { sendError(req, "Emergency stop active"); return; }

            DynamicJsonDocument body(512);
            if (deserializeJson(body, data, len)) { sendError(req, "Invalid JSON"); return; }

            const char* name = body["name"] | "";
            DynamicJsonDocument recipes = storageReadJson(RECIPES_PATH, 8192);
            if (recipes.isNull()) { sendError(req, "No recipes found"); return; }

            for (JsonObject recipe : recipes["recipes"].as<JsonArray>()) {
                if (strcmp(recipe["name"] | "", name) == 0) {
                    JsonArray steps = recipe["steps"].as<JsonArray>();
                    int count = steps.size();
                    RecipeStep rSteps[10];
                    int i = 0;
                    for (JsonObject step : steps) {
                        if (i >= 10) break;
                        rSteps[i].pump = (int)step["pump"] - 1;
                        rSteps[i].ml   = step["ml"];
                        i++;
                    }
                    dispenseRecipe(rSteps, i);
                    displaySetLastAction(String("Recipe: ") + name);
                    sendOk(req, "dispensing");
                    return;
                }
            }
            sendError(req, "Recipe not found");
        }
    );

    // ── POST /api/pour/manual ────────────────────────────────────────────────
    server.on("/api/pour/manual", HTTP_POST, [](AsyncWebServerRequest* req) {
        if (emergencyStop) { sendError(req, "Emergency stop active"); return; }

        if (!req->hasParam("pump") || !req->hasParam("ml")) {
            sendError(req, "Missing pump or ml param");
            return;
        }
        int pump = req->getParam("pump")->value().toInt() - 1;
        float ml = req->getParam("ml")->value().toFloat();

        if (pump < 0 || pump >= NUM_PUMPS || ml <= 0) {
            sendError(req, "Invalid pump or ml");
            return;
        }
        if (!dispensePump(pump, ml)) {
            sendError(req, "Pump busy or not ready");
            return;
        }
        displaySetLastAction(String("Manual P") + (pump + 1) + " " + String(ml, 0) + "ml");
        sendOk(req, "dispensing");
    });

    // ── POST /api/stop ───────────────────────────────────────────────────────
    server.on("/api/stop", HTTP_POST, [](AsyncWebServerRequest* req) {
        stopAllPumps();
        displaySetLastAction("API stop");
        sendOk(req, "stopped");
    });

    // ── POST /api/calibrate/start ────────────────────────────────────────────
    server.on("/api/calibrate/start", HTTP_POST, [](AsyncWebServerRequest* req) {
        if (!req->hasParam("pump")) { sendError(req, "Missing pump param"); return; }
        int pump = req->getParam("pump")->value().toInt() - 1;
        if (!calibrateStart(pump)) { sendError(req, "Cannot start calibration"); return; }
        displaySetLastAction(String("Cal start P") + (pump + 1));
        sendOk(req, "calibration started");
    });

    // ── POST /api/calibrate/save ──────────────────────────────────────────────
    server.on("/api/calibrate/save", HTTP_POST, [](AsyncWebServerRequest* req) {
        if (!req->hasParam("pump") || !req->hasParam("ml")) {
            sendError(req, "Missing pump or ml param");
            return;
        }
        int pump = req->getParam("pump")->value().toInt() - 1;
        float ml = req->getParam("ml")->value().toFloat();
        if (pump < 0 || pump >= NUM_PUMPS || ml <= 0) {
            sendError(req, "Invalid params");
            return;
        }
        calibrateSave(pump, ml);
        savePumpConfigsToStorage();
        displaySetLastAction(String("Cal saved P") + (pump + 1));
        sendOk(req, "calibration saved");
    });

    // 404 fallback
    server.onNotFound([](AsyncWebServerRequest* req) {
        req->send(404, "text/plain", "Not found");
    });

    server.begin();
    Serial.println("[WebServer] Started on port 80");
}
