#ifndef STORAGE_MANAGER_H
#define STORAGE_MANAGER_H

#include <Arduino.h>
#include <SPI.h>
#include <SD.h>
#include "Config.h"
#include "MidiManager.h"

class StorageManager {
public:
    static void* labelsCacheDoc;
    static bool init();
    static void listFiles();
    
    // Configuration persistence
    static bool saveConfig();
    static bool loadConfig();
    static bool migrateBinaryToJSON();
    
    // Label persistence
    static bool saveLabels(); // Legacy/Full save
    static bool loadLabels(); // Legacy/Full load
    static void initLabels();
    static void loadLabelsForCurrentState();
    static void loadMixerLabelsForCurrentState();
    static void saveLabel(int page, int channel, int pot, String label);
    static String getLabel(int page, int channel, int pot);
    
    static const int LABEL_RECORD_SIZE = 32;
    
    // Auto-save logic
    static void handleAutoSave();

private:
    static unsigned long lastSaveTime;
    static const unsigned long AUTO_SAVE_INTERVAL = 30000;
};

#endif // STORAGE_MANAGER_H
