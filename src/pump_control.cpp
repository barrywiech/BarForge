#include "pump_control.h"
#include "config.h"
#include <Arduino.h>

// ─── GPIO map (0-based index → pin) ──────────────────────────────────────────
static const uint8_t PUMP_PINS[NUM_PUMPS] = {
    PUMP1_PIN, PUMP2_PIN, PUMP3_PIN, PUMP4_PIN, PUMP5_PIN
};

// ─── State ────────────────────────────────────────────────────────────────────
PumpConfig pumpConfigs[NUM_PUMPS];
volatile bool emergencyStop = false;

static unsigned long pumpStartMs[NUM_PUMPS] = {0};
static unsigned long pumpDurationMs[NUM_PUMPS] = {0};
static bool          pumpActive[NUM_PUMPS] = {false};

static String lastAction = "Boot";

// ─── Init ─────────────────────────────────────────────────────────────────────
void pumpsInit() {
    for (int i = 0; i < NUM_PUMPS; i++) {
        pinMode(PUMP_PINS[i], OUTPUT);
        digitalWrite(PUMP_PINS[i], HIGH);  // HIGH = relay OFF (active-LOW SSR)

        // Default config
        snprintf(pumpConfigs[i].spirit, sizeof(pumpConfigs[i].spirit), "Pump %d", i + 1);
        pumpConfigs[i].bottleSize = 700.0f;
        pumpConfigs[i].remaining  = 700.0f;
        pumpConfigs[i].mlPerSec   = DEFAULT_ML_PER_SEC;
    }

    // Emergency stop — input-only pin, external pull-up
    pinMode(ESTOP_PIN, INPUT);
    attachInterrupt(digitalPinToInterrupt(ESTOP_PIN), onEmergencyStop, FALLING);

    Serial.println("[Pumps] Initialized");
}

// ─── ISR ──────────────────────────────────────────────────────────────────────
void IRAM_ATTR onEmergencyStop() {
    emergencyStop = true;
    // Drive all relay pins HIGH (off) directly — safe to do in IRAM ISR
    for (int i = 0; i < NUM_PUMPS; i++) {
        digitalWrite(PUMP_PINS[i], HIGH);
        pumpActive[i] = false;
    }
}

// ─── Raw relay control ────────────────────────────────────────────────────────
void startPump(int id) {
    if (id < 0 || id >= NUM_PUMPS) return;
    if (emergencyStop) return;
    digitalWrite(PUMP_PINS[id], LOW);   // active-LOW
    pumpActive[id] = true;
}

void stopPump(int id) {
    if (id < 0 || id >= NUM_PUMPS) return;
    digitalWrite(PUMP_PINS[id], HIGH);
    pumpActive[id] = false;
    pumpDurationMs[id] = 0;
}

void stopAllPumps() {
    for (int i = 0; i < NUM_PUMPS; i++) {
        stopPump(i);
    }
    lastAction = "Emergency stop";
}

// ─── Non-blocking dispense ────────────────────────────────────────────────────
bool dispensePump(int id, float ml) {
    if (id < 0 || id >= NUM_PUMPS) return false;
    if (emergencyStop) return false;
    if (pumpActive[id]) return false;
    if (pumpConfigs[id].mlPerSec <= 0.0f) return false;

    unsigned long durationMs = (unsigned long)((ml / pumpConfigs[id].mlPerSec) * 1000.0f);
    pumpStartMs[id]    = millis();
    pumpDurationMs[id] = durationMs;
    startPump(id);

    Serial.printf("[Pumps] Dispensing pump %d: %.1f ml (~%lu ms)\n", id + 1, ml, durationMs);
    return true;
}

// ─── loop() update ────────────────────────────────────────────────────────────
void pumpsUpdate() {
    if (emergencyStop) return;

    unsigned long now = millis();
    for (int i = 0; i < NUM_PUMPS; i++) {
        if (pumpActive[i] && pumpDurationMs[i] > 0) {
            if (now - pumpStartMs[i] >= pumpDurationMs[i]) {
                // Timer elapsed — stop and track volume
                float elapsed = pumpDurationMs[i] / 1000.0f;
                float dispensed = elapsed * pumpConfigs[i].mlPerSec;
                pumpConfigs[i].remaining -= dispensed;
                if (pumpConfigs[i].remaining < 0.0f) pumpConfigs[i].remaining = 0.0f;

                stopPump(i);
                Serial.printf("[Pumps] Pump %d done. Remaining: %.0f ml\n",
                              i + 1, pumpConfigs[i].remaining);
            }
        }
    }
}

// ─── Calibration ─────────────────────────────────────────────────────────────
bool calibrateStart(int id) {
    if (id < 0 || id >= NUM_PUMPS) return false;
    if (pumpActive[id]) return false;

    unsigned long durationMs = (unsigned long)(CALIBRATION_RUN_SECS * 1000.0f);
    pumpStartMs[id]    = millis();
    pumpDurationMs[id] = durationMs;
    startPump(id);

    Serial.printf("[Pumps] Calibration run pump %d (%.0f s)\n", id + 1, CALIBRATION_RUN_SECS);
    return true;
}

void calibrateSave(int id, float measuredMl) {
    if (id < 0 || id >= NUM_PUMPS) return;
    pumpConfigs[id].mlPerSec = measuredMl / CALIBRATION_RUN_SECS;
    Serial.printf("[Pumps] Pump %d calibrated: %.3f ml/s\n",
                  id + 1, pumpConfigs[id].mlPerSec);
}

// ─── Recipe dispense ─────────────────────────────────────────────────────────
void dispenseRecipe(const RecipeStep* steps, int count) {
    for (int i = 0; i < count; i++) {
        dispensePump(steps[i].pump, steps[i].ml);
    }
}

// ─── Status ───────────────────────────────────────────────────────────────────
bool isPumpActive(int id) {
    if (id < 0 || id >= NUM_PUMPS) return false;
    return pumpActive[id];
}

bool isAnyPumpActive() {
    for (int i = 0; i < NUM_PUMPS; i++) {
        if (pumpActive[i]) return true;
    }
    return false;
}

String getStatusString() {
    return isAnyPumpActive() ? "DISPENSING" : "IDLE";
}
