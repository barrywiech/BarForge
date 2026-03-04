#include "display.h"
#include "config.h"
#include "pump_control.h"
#include <TFT_eSPI.h>
#include <WiFi.h>

// ─── TFT instance ─────────────────────────────────────────────────────────────
static TFT_eSPI tft = TFT_eSPI();

// ─── Colours ──────────────────────────────────────────────────────────────────
#define C_BG       0x0841   // very dark grey
#define C_ACCENT   0x07FF   // cyan
#define C_WHITE    TFT_WHITE
#define C_GREEN    0x07E0
#define C_YELLOW   0xFFE0
#define C_RED      TFT_RED
#define C_DARK     0x2104
#define C_PANEL    0x10A2

// ─── Layout constants ─────────────────────────────────────────────────────────
#define SCREEN_W   480
#define SCREEN_H   320
#define TOP_BAR_H  36
#define STATUS_H   30
#define BAR_AREA_Y (TOP_BAR_H + STATUS_H + 4)
#define BAR_H      44
#define BAR_SPACING 6
#define LAST_ACT_Y (SCREEN_H - 20)

// ─── State ────────────────────────────────────────────────────────────────────
static String currentStatus = "IDLE";
static String lastAction     = "Ready";

static String  prevStatus = "";
static String  prevIP     = "";
static String  prevLastAction = "";
static float   prevRemaining[NUM_PUMPS] = {-1, -1, -1, -1, -1};
static bool    prevPumpActive[NUM_PUMPS] = {false, false, false, false, false};

// ─── Helpers ─────────────────────────────────────────────────────────────────
static uint16_t levelColour(float pct) {
    if (pct > 0.5f) return C_GREEN;
    if (pct > 0.2f) return C_YELLOW;
    return C_RED;
}

static void drawTopBar(const String& ip) {
    tft.fillRect(0, 0, SCREEN_W, TOP_BAR_H, C_ACCENT);
    tft.setTextColor(C_BG, C_ACCENT);
    tft.setTextSize(2);
    tft.setCursor(8, 10);
    tft.print("BarForge");
    tft.setTextSize(1);
    tft.setCursor(SCREEN_W - 130, 13);
    tft.print("IP: ");
    tft.print(ip);
}

static void drawStatusBar(const String& status) {
    uint16_t col = (status == "IDLE") ? C_DARK : C_RED;
    tft.fillRect(0, TOP_BAR_H, SCREEN_W, STATUS_H, col);
    tft.setTextColor(C_WHITE, col);
    tft.setTextSize(2);
    tft.setCursor(8, TOP_BAR_H + 7);
    tft.print("Status: ");
    tft.print(status);
}

static void drawBottleBar(int idx) {
    const PumpConfig& p = pumpConfigs[idx];
    int y = BAR_AREA_Y + idx * (BAR_H + BAR_SPACING);

    // Background panel
    tft.fillRect(0, y, SCREEN_W, BAR_H, C_PANEL);

    // Label
    tft.setTextColor(C_WHITE, C_PANEL);
    tft.setTextSize(1);
    tft.setCursor(4, y + 4);
    tft.printf("P%d  %-12s", idx + 1, p.spirit);

    // Volume text
    tft.setCursor(4, y + 16);
    tft.printf("%.0f / %.0f ml", p.remaining, p.bottleSize);

    // Active indicator dot
    uint16_t dotCol = isPumpActive(idx) ? C_ACCENT : C_DARK;
    tft.fillCircle(SCREEN_W - 12, y + BAR_H / 2, 6, dotCol);

    // Level bar
    float pct = (p.bottleSize > 0) ? (p.remaining / p.bottleSize) : 0.0f;
    if (pct > 1.0f) pct = 1.0f;
    int barX  = 180;
    int barW  = SCREEN_W - barX - 24;
    int fillW = (int)(pct * barW);

    tft.fillRect(barX, y + 10, barW, 24, C_DARK);
    if (fillW > 0) {
        tft.fillRect(barX, y + 10, fillW, 24, levelColour(pct));
    }
    // Percentage label
    tft.setTextColor(C_WHITE, levelColour(pct));
    if (fillW > 28) {
        tft.setCursor(barX + fillW - 28, y + 16);
        tft.printf("%3.0f%%", pct * 100.0f);
    }
}

static void drawLastAction(const String& action) {
    tft.fillRect(0, LAST_ACT_Y - 2, SCREEN_W, 22, C_BG);
    tft.setTextColor(0xAD55, C_BG);
    tft.setTextSize(1);
    tft.setCursor(4, LAST_ACT_Y);
    tft.print("Last: ");
    tft.print(action);
}

// ─── Public API ───────────────────────────────────────────────────────────────
void displayInit() {
    pinMode(TFT_BL, OUTPUT);
    digitalWrite(TFT_BL, HIGH);

    tft.init();
    tft.setRotation(1);   // landscape
    tft.fillScreen(C_BG);

    String ip = WiFi.localIP().toString();
    if (ip == "0.0.0.0") ip = "connecting...";
    drawTopBar(ip);
    drawStatusBar("IDLE");
    for (int i = 0; i < NUM_PUMPS; i++) drawBottleBar(i);
    drawLastAction("Ready");

    Serial.println("[Display] Initialized");
}

void displayUpdate() {
    String ip = WiFi.localIP().toString();
    if (ip == "0.0.0.0") ip = "connecting...";

    String status = getStatusString();
    if (emergencyStop) status = "ESTOP!";

    // Top bar — refresh only if IP changed
    if (ip != prevIP) {
        drawTopBar(ip);
        prevIP = ip;
    }

    // Status bar
    if (status != prevStatus) {
        drawStatusBar(status);
        prevStatus = status;
    }

    // Bottle bars — refresh only changed rows
    for (int i = 0; i < NUM_PUMPS; i++) {
        bool activeNow = isPumpActive(i);
        if (pumpConfigs[i].remaining != prevRemaining[i] ||
            activeNow != prevPumpActive[i]) {
            drawBottleBar(i);
            prevRemaining[i]  = pumpConfigs[i].remaining;
            prevPumpActive[i] = activeNow;
        }
    }

    // Last action
    if (lastAction != prevLastAction) {
        drawLastAction(lastAction);
        prevLastAction = lastAction;
    }
}

void displaySetStatus(const String& status) {
    currentStatus = status;
}

void displaySetLastAction(const String& action) {
    lastAction = action;
}
