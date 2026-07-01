#include <Wire.h>
#include <Control_Surface.h>
#include <WiFi.h>
#include <WebServer.h>
#include "esp_bt.h"

#include "Config.h"
#include "HardwareHAL.h"
#include "LfoEngine.h"
#include "MidiManager.h"
#include "StorageManager.h"
#include "UiManager.h"
#include "WebServerManager.h"
#include "SensorManager.h"
#include "TouchButtons.h"
#include "src/UI/ui.h"

// ========== SETUP ==========
void setup() {
    Serial.begin(115200);
    Serial.println("\n\n=== MIDI CONTROLLER BOOTING ===");

    MidiManager::init();        // Also inits BLE MIDI + Control_Surface
    if (StorageManager::init()) {
        StorageManager::loadConfig();
        StorageManager::initLabels();
        StorageManager::loadLabels();
    }
    WebServerManager::init();
    UiManager::init();
    HardwareHAL::init();
    LfoEngine::init();
    SensorManager::init();
    TouchButtons::begin(TouchButtons::handleTouch);

    UiManager::loadValuesForCurrentState();
    UiManager::updateChannelButtonColors();
    UiManager::updatePageButtonColors();
    UiManager::updateLFOButtonColors();
    UiManager::updateParameterLabels();
    UiManager::setActiveArc(1);

    MidiManager::selectedArcForModulation = 1;
    MidiManager::modulationTargetPage = MidiManager::currentPage;
    MidiManager::modulationTargetChannel = MidiManager::currentMidiChannel;
    UiManager::updateModulationUIColors();

    Serial.println("Setup complete - Ready!");
}

// ========== LOOP ==========
void loop() {
    HardwareHAL::updateVibration();
    SensorManager::update();
    LfoEngine::update();

    LfoEngine::lfos[0].updateDisplay(ui_ButtonLFO1);
    LfoEngine::lfos[1].updateDisplay(ui_ButtonLFO2);
    LfoEngine::lfos[2].updateDisplay(ui_ButtonLFO3);
    LfoEngine::lfos[3].updateDisplay(ui_ButtonLFO4);

    float mixedValue = LfoEngine::getMixedValue();
    UiManager::updateMainLFODisplay();
    UiManager::update();
    UiManager::updateSelectedArcMod(mixedValue);
    MidiManager::updateModulation(mixedValue);

    MidiManager::update();
    Control_Surface.loop();
    WebServerManager::handleWiFi();
    StorageManager::handleAutoSave();
    TouchButtons::update();

    vTaskDelay(1);
}