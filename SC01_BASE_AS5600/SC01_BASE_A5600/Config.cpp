#include "Config.h"

const WifiNetwork wifiNetworks[] = {
    {"JUE_BAT", "azertyuiop"},
    {"FCI", "1Moutonmangeleloup"},
    {"B_A_T", "azertyuiop"}
};
const int numWifiNetworks = sizeof(wifiNetworks) / sizeof(wifiNetworks[0]);

const char *ssid = wifiNetworks[0].ssid;
const char *password = wifiNetworks[0].password;