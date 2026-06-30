#ifndef WEB_SERVER_MANAGER_H
#define WEB_SERVER_MANAGER_H

#include <WiFi.h>
#include <WebServer.h>
#include "Config.h"
#include "MidiManager.h"
#include "StorageManager.h"
#include "UiManager.h"

class WebServerManager {
public:
    static void init();
    static void handleWiFi();
    static void startWiFi();
    static void stopWiFi();
    
    static bool isWiFiEnabled() { return wifiEnabled; }
    static bool isWiFiConnected() { return wifiConnected; }

private:
    static void handleWebRoot();
    static void handleWebAPI();
    static void handleButtonsAPI();
    static void handleMixerConfigAPI();
    static void handleTrackActionsAPI();
    static void handleTemplateActionsAPI();
    static void handleFileList();
    static void handleFileDownload();
    static void handleFileDelete();
    static void handleFileUpload();
    static void handleFactoryReset();
    static String getIndexHtml();

    static WebServer webServer;
    static File uploadFile;
    static bool wifiEnabled;
    static bool wifiConnected;
    static unsigned long wifiConnectStartTime;
    static const unsigned long wifiConnectTimeout = 10000;
};

#endif // WEB_SERVER_MANAGER_H
