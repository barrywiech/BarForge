// BarForge — TFT_eSPI User_Setup.h
// ILI9488 3.5" display over SPI on ESP32 Wroom-32
//
// Place this file in lib/TFT_eSPI/ so PlatformIO picks it up ahead of the
// library's own User_Setup.h.  The build flag below tells TFT_eSPI to use
// this file instead of its default.
//
// Add to platformio.ini build_flags if needed:
//   build_flags = -DUSER_SETUP_LOADED

#define USER_SETUP_LOADED

// ─── Driver ──────────────────────────────────────────────────────────────────
#define ILI9488_DRIVER

// ─── SPI pins ────────────────────────────────────────────────────────────────
#define TFT_MISO  19   // SDO on display (optional — display is write-only)
#define TFT_MOSI  23   // SDI
#define TFT_SCLK  18   // SCK
#define TFT_CS     5   // CS
#define TFT_DC    16   // O/C (Data/Command)
#define TFT_RST   17   // RST
// Backlight (BL) is managed by firmware via GPIO 21; not defined here.

// ─── Display dimensions ───────────────────────────────────────────────────────
// ILI9488 is 480×320 in landscape.  TFT_eSPI uses native portrait dimensions;
// rotation is applied in firmware via tft.setRotation(1).
#define TFT_WIDTH  320
#define TFT_HEIGHT 480

// ─── SPI frequency ────────────────────────────────────────────────────────────
#define SPI_FREQUENCY        27000000   // 27 MHz — safe for ILI9488
#define SPI_READ_FREQUENCY    5000000

// ─── Colour order ─────────────────────────────────────────────────────────────
// ILI9488 is RGB — swap bytes if colours look wrong
// #define TFT_RGB_ORDER TFT_RGB   // uncomment if colours are swapped

// ─── Fonts ────────────────────────────────────────────────────────────────────
#define LOAD_GLCD    // Font 1 (built-in 5×7)
#define LOAD_FONT2   // Font 2 (8-pixel)
#define LOAD_FONT4   // Font 4 (15-pixel)
#define SMOOTH_FONT
