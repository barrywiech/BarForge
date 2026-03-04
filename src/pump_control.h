#pragma once
#include <stdint.h>

// ─── Pump state ───────────────────────────────────────────────────────────────
struct PumpConfig {
    char   spirit[32];
    float  bottleSize;    // ml
    float  remaining;     // ml
    float  mlPerSec;      // calibration factor
};

extern PumpConfig pumpConfigs[5];
extern volatile bool emergencyStop;

// ─── Init / ISR ───────────────────────────────────────────────────────────────
void pumpsInit();
void IRAM_ATTR onEmergencyStop();

// ─── Control ─────────────────────────────────────────────────────────────────
void startPump(int id);   // id: 0-based (0–4)
void stopPump(int id);
void stopAllPumps();

// Kick off a non-blocking dispense; returns false if pump already running.
bool dispensePump(int id, float ml);

// Must be called from loop() every iteration.
void pumpsUpdate();

// ─── Calibration ─────────────────────────────────────────────────────────────
// Starts calibration run (pump runs for CALIBRATION_RUN_SECS).
bool calibrateStart(int id);

// Save measured ml → compute and store mlPerSec.
void calibrateSave(int id, float measuredMl);

// ─── Recipe dispense ─────────────────────────────────────────────────────────
struct RecipeStep {
    int   pump;  // 0-based
    float ml;
};

// Queue a recipe (simultaneous multi-pump). steps array, count = number of steps.
void dispenseRecipe(const RecipeStep* steps, int count);

// ─── Status ───────────────────────────────────────────────────────────────────
bool isPumpActive(int id);
bool isAnyPumpActive();
String getStatusString();   // "IDLE" or "DISPENSING"
