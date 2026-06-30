#ifndef CONFIG_H
#define CONFIG_H

#include <Arduino.h>

// ========== WIFI CONFIGURATION ==========
struct WifiNetwork {
    const char *ssid;
    const char *password;
};
extern const WifiNetwork wifiNetworks[];
extern const int numWifiNetworks;
extern const char *ssid;
extern const char *password;
const unsigned long WIFI_CONNECT_TIMEOUT = 10000;

// ========== HARDWARE PINS ==========
static const int PIN_TOUCH_1 = 12;
static const int PIN_TOUCH_2 = 14;
static const int PIN_SCL = 11;
static const int PIN_SDA = 10;

static const int PIN_SD_SCK = 39;
static const int PIN_SD_MISO = 38;
static const int PIN_SD_MOSI = 40;
static const int PIN_SD_CS = 41;

static const int PIN_VIBRATION_MOTOR = 13;
static const int VIBRATION_DURATION_MS = 1;

static const float MAX_LFO_RATE = 2.0f;
static const float MAX_LFO_DEPTH = 5.0f;
static const int LFO_OFFSET_DETENT_MIN = 63;
static const int LFO_OFFSET_DETENT_MAX = 65;

// ========== SCREEN CONFIGURATION ==========
#define SCREEN_WIDTH 480
#define SCREEN_HEIGHT 320
#define SCREEN_BRIGHTNESS 200  // Backlight intensity (0 = off, 255 = max)

// ========== SYSTEM LIMITS ==========
#define NUM_PAGES 8
#define NUM_POTS 20
#define NUM_CHANNELS 16
#define NUM_ARCS 16

// ========== I2C ADDRESSES ==========
#define TCA9548A_ADDR_1 0x71
#define TCA9548A_ADDR_2 0x70
#define TCA9548A_ADDR_3 0x72
#define AS5600_ADDR 0x36

// ========== MUX PORT MAPPINGS ==========
const int MUX_PORTS_TCA1[] = { 2, 3, 5, 6, 0, 1, 4, 7 };
const int MUX_PORTS_TCA2[] = { 6, 5, 1, 0, 7, 4, 3, 2 };
const int MUX_PORTS_TCA3[] = { 4, 3, 2, 7 };

#endif // CONFIG_H
