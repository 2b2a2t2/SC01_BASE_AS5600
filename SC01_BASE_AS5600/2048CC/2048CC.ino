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

// ========== GLOBAL INSTANCES ==========
BluetoothMIDI_Interface midi_ble;


// ========== MIDI CALLBACKS ==========
struct MyMIDI_Callbacks : FineGrainedMIDI_Callbacks<MyMIDI_Callbacks> {
    void onControlChange(Channel channel, uint8_t controller, uint8_t value, Cable cable) {
        Serial.printf("MIDI RX: Ch %d, CC %d, Val %d\n", channel.getRaw() + 1, controller, value);
        MidiManager::handleIncomingMIDI(channel, controller, value);
    }
} midi_callbacks;

// ========== FORWARD DECLARATIONS ==========
void OtherCoreStuff(void *pvParameters);
void updateModulatedArcMIDI();

// Touch event handler
void handleTouch(int pin, bool isTouched) {
    static unsigned long pressStartTime = 0;

    if (pin == 12) {
        UiManager::isShiftActive = isTouched;
        
        if (isTouched) {
            // PRESS: Enter Momentary Mixer Mode
            pressStartTime = millis();
            UiManager::shiftWasUsed = false;
            UiManager::previousMixerMode = UiManager::isMixerMode;
            UiManager::isMixerMode = true;
            UiManager::loadValuesForCurrentState();
            UiManager::updateParameterLabels();
            UiManager::updateLFOButtonColors();
        } else {
            // RELEASE: Check duration
            unsigned long duration = millis() - pressStartTime;
            if (duration < 300) {
                // TAP: Always Exit Mixer mode
                UiManager::isMixerMode = false;
                UiManager::loadValuesForCurrentState();
                UiManager::updateParameterLabels();
                UiManager::updateLFOButtonColors();
            } else {
                // HOLD: Revert Mixer Mode if shift wasn't used for a selection
                if (!UiManager::shiftWasUsed) {
                    UiManager::isMixerMode = UiManager::previousMixerMode;
                    UiManager::loadValuesForCurrentState();
                    UiManager::updateParameterLabels();
                    UiManager::updateLFOButtonColors();
                }
            }
        }
        return;
    }

    if (isTouched) {
        if (pin == 14) {
            UiManager::toggleLfoMode();
        }
    }
}

// ========== SETUP ==========
void setup() {
    Serial.begin(115200);
    Serial.println("\n\n=== MIDI CONTROLLER BOOTING ===");

    // Initialize MIDI Manager defaults
    MidiManager::init();

    // Initialize Storage
    if (StorageManager::init()) {
        StorageManager::listFiles();
        if (StorageManager::loadConfig()) {
            Serial.println("Loaded configuration from SD card");
        }
        StorageManager::initLabels();
        StorageManager::loadLabels();
    }

    // Initialize Managers
    WebServerManager::init();
    UiManager::init();
    HardwareHAL::init();
    LfoEngine::init();
    SensorManager::init();
    TouchButtons::begin(handleTouch);

    // Initialize MIDI
    midi_ble.setName("BAT LiL");
    midi_ble.setAsDefault();
    Control_Surface.begin();
    midi_ble.setCallbacks(midi_callbacks);

    // Initial UI Sync
    UiManager::loadValuesForCurrentState();
    UiManager::updateChannelButtonColors();
    UiManager::updatePageButtonColors();
    UiManager::updateLFOButtonColors();
    UiManager::updateParameterLabels();
    UiManager::setActiveArc(1);
    
    // Set default modulation target
    MidiManager::selectedArcForModulation = 1;
    MidiManager::modulationTargetPage = MidiManager::currentPage;
    MidiManager::modulationTargetChannel = MidiManager::currentMidiChannel;
    UiManager::updateModulationUIColors();

    // Secondary Core Task
    xTaskCreatePinnedToCore(OtherCoreStuff, "OtherCoreStuff", 4000, NULL, 0, NULL, 1);

    Serial.println("Setup complete - Ready!");
}

// ========== LOOP ==========
void loop() {
    HardwareHAL::updateVibration();
    SensorManager::update();
    LfoEngine::update();
    
    // Sync LFO display
    LfoEngine::lfos[0].updateDisplay(ui_ButtonLFO1);
    LfoEngine::lfos[1].updateDisplay(ui_ButtonLFO2);
    LfoEngine::lfos[2].updateDisplay(ui_ButtonLFO3);
    LfoEngine::lfos[3].updateDisplay(ui_ButtonLFO4);

    float mixedValue = LfoEngine::getMixedValue();
    UiManager::updateMainLFODisplay();
    
    // Update UI and MIDI modulation
    UiManager::update();
    UiManager::updateSelectedArcMod(mixedValue);
    updateModulatedArcMIDI();
    
    // Background tasks
    midi_ble.update();
    Control_Surface.loop();
    WebServerManager::handleWiFi();
    StorageManager::handleAutoSave();
    TouchButtons::update();
}

// Sensor polling moved to SensorManager

// ========== MODULATION MIDI ENGINE ==========
void updateModulatedArcMIDI() {
    int arcIndex = MidiManager::selectedArcForModulation - 1;
    if (arcIndex < 0 || arcIndex >= 16) return;

    int targetPage = MidiManager::modulationTargetPage;
    int targetChannel = MidiManager::modulationTargetChannel;

    // Check if modulation is active and mix amounts are non-zero
    bool hasActiveModulation = MidiManager::modulationEnabled && !LfoEngine::isKilled && 
                              (LfoEngine::mixAmounts[0] > 0.0f || 
                               LfoEngine::mixAmounts[1] > 0.0f || 
                               LfoEngine::mixAmounts[2] > 0.0f || 
                               LfoEngine::mixAmounts[3] > 0.0f);

    static int lastSentModulatedValue[16] = { -1 };
    static bool wasModulating[16] = { false };

    if (hasActiveModulation) {
        int baseValue = MidiManager::storedMidiCCValues[targetPage][targetChannel][arcIndex];
        float mixedValue = LfoEngine::getMixedValue();
        float offset = (mixedValue - 0.5f) * 127.0f;
        int finalMidiValue = constrain((int)(baseValue + offset), 0, 127);

        if (finalMidiValue != lastSentModulatedValue[arcIndex]) {
            MidiManager::sendModulatedCC(arcIndex, finalMidiValue, targetPage, targetChannel);
            lastSentModulatedValue[arcIndex] = finalMidiValue;
        }
        wasModulating[arcIndex] = true;
    } else {
        // If it was modulating, send one final CC to restore the base value in the DAW
        if (wasModulating[arcIndex]) {
            int baseValue = MidiManager::storedMidiCCValues[targetPage][targetChannel][arcIndex];
            MidiManager::sendModulatedCC(arcIndex, baseValue, targetPage, targetChannel);
            lastSentModulatedValue[arcIndex] = baseValue;
            wasModulating[arcIndex] = false;
        }
    }
}

// ========== CORE 1 TASK ==========
void OtherCoreStuff(void *pvParameters) {
    for (;;) {
        vTaskDelay(1);
    }
}

// ========== LVGL EVENT HANDLERS (Delegated to Managers) ==========
void ui_event_ButtonChannelGeneric(lv_event_t *e) {
    lv_event_code_t code = lv_event_get_code(e);
    lv_obj_t* target = lv_event_get_target(e);
    int channelIndex = (int)(intptr_t)lv_obj_get_user_data(target) - 1;

    if (code == LV_EVENT_PRESSED) {
        uint8_t note = MidiManager::channelButtonNotes[channelIndex];
        uint8_t channel = MidiManager::channelButtonTargetChannel;
        Control_Surface.sendNoteOn({note, (Channel)(channel + 1)}, 127);
    } else if (code == LV_EVENT_RELEASED) {
        uint8_t note = MidiManager::channelButtonNotes[channelIndex];
        uint8_t channel = MidiManager::channelButtonTargetChannel;
        Control_Surface.sendNoteOff({note, (Channel)(channel + 1)}, 0);
    } else if (code == LV_EVENT_CLICKED) {
        if (channelIndex != MidiManager::currentMidiChannel) {
            MidiManager::setMidiChannel(channelIndex);
            if (UiManager::isLfoMode) UiManager::isMixerMode = false;
            UiManager::updateChannelButtonColors();
            UiManager::updatePageButtonColors();
            UiManager::updateLFOButtonColors();
            UiManager::loadValuesForCurrentState();
            UiManager::updateParameterLabels();
        }
    }
}

void ui_event_ButtonPageGeneric(lv_event_t *e) {
    if (lv_event_get_code(e) == LV_EVENT_CLICKED) {
        int page = (int)(intptr_t)lv_obj_get_user_data(lv_event_get_target(e));
        int newPage = page - 1;
        if (newPage != MidiManager::currentPage) {
            MidiManager::setPage(newPage);
            if (UiManager::isLfoMode) UiManager::isMixerMode = false; 
            UiManager::updatePageButtonColors();
            UiManager::updateLFOButtonColors(); // Refresh LFO switch state if it was linked to Mixer
            UiManager::loadValuesForCurrentState();
            UiManager::updateParameterLabels();
        }
    }
}

void ui_event_ButtonArcGeneric(lv_event_t *e) {
    if (lv_event_get_code(e) == LV_EVENT_CLICKED) {
        int index = (int)(intptr_t)lv_obj_get_user_data(lv_event_get_target(e));
        UiManager::setActiveArc(index);
        UiManager::updateModulationUIColors();
    }
}

void ui_event_ButtonArcModulationSelect(lv_event_t *e) {
    if (lv_event_get_code(e) == LV_EVENT_LONG_PRESSED) {
        int index = (int)(intptr_t)lv_obj_get_user_data(lv_event_get_target(e));
        UiManager::setModulationTarget(index);
    }
}

void ui_event_ArcGeneric(lv_event_t *e) { /* Managed by UiManager internally */ }

void lfo1EventHandler(lv_event_t *e) { 
    if (lv_event_get_code(e) == LV_EVENT_CLICKED) { 
        if (UiManager::currentMenuState == UiManager::MENU_KEYBOARD) {
            MidiManager::keyboardOctave--;
            if (MidiManager::keyboardOctave < -3) MidiManager::keyboardOctave = -3;
            UiManager::updateLFOButtonColors();
            return;
        }
        if (UiManager::isShiftActive) { UiManager::setMixerPage(1); return; }
        if (UiManager::isLfoMode) {
            LfoEngine::currentLfoIndex = 0; LfoEngine::mixMode = false;
            UiManager::syncLfoArcValues();
        } else {
            UiManager::setCcRow(0);
        }
        UiManager::updateLFOButtonColors(); 
    } 
}
void lfo2EventHandler(lv_event_t *e) { 
    if (lv_event_get_code(e) == LV_EVENT_CLICKED) { 
        if (UiManager::currentMenuState == UiManager::MENU_KEYBOARD) {
            MidiManager::keyboardOctave++;
            if (MidiManager::keyboardOctave > 3) MidiManager::keyboardOctave = 3;
            UiManager::updateLFOButtonColors();
            return;
        }
        if (UiManager::isShiftActive) { UiManager::setMixerPage(2); return; }
        if (UiManager::isLfoMode) {
            LfoEngine::currentLfoIndex = 1; LfoEngine::mixMode = false;
            UiManager::syncLfoArcValues();
        } else {
            UiManager::setCcRow(1);
        }
        UiManager::updateLFOButtonColors(); 
    } 
}
void lfo3EventHandler(lv_event_t *e) { 
    if (lv_event_get_code(e) == LV_EVENT_CLICKED) { 
        if (UiManager::currentMenuState == UiManager::MENU_KEYBOARD) {
            MidiManager::keyboardOctave = 0;
            UiManager::updateLFOButtonColors();
            return;
        }
        if (UiManager::isShiftActive) { UiManager::setMixerPage(3); return; }
        if (UiManager::isLfoMode) {
            LfoEngine::currentLfoIndex = 2; LfoEngine::mixMode = false;
            UiManager::syncLfoArcValues();
        } else {
            UiManager::setCcRow(2);
        }
        UiManager::updateLFOButtonColors(); 
    } 
}
void lfo4EventHandler(lv_event_t *e) { 
    if (lv_event_get_code(e) == LV_EVENT_CLICKED) { 
        if (UiManager::currentMenuState == UiManager::MENU_KEYBOARD) return;
        if (UiManager::isShiftActive) { UiManager::setMixerPage(4); return; }
        if (UiManager::isLfoMode) {
            LfoEngine::currentLfoIndex = 3; LfoEngine::mixMode = false;
            UiManager::syncLfoArcValues();
        } else {
            UiManager::setCcRow(3);
        }
        UiManager::updateLFOButtonColors(); 
    } 
}
void lfoMixEventHandler(lv_event_t *e) { 
    lv_event_code_t code = lv_event_get_code(e);

    if (code == LV_EVENT_PRESSED) {
        if (!UiManager::isLfoMode) {
            uint8_t note = UiManager::isMixerMode ? MidiManager::channelButtonNotes[MidiManager::currentMidiChannel] : MidiManager::mixerButtonNote;
            Control_Surface.sendNoteOn({note, (Channel)(MidiManager::channelButtonTargetChannel + 1)}, 127);
        }
    } else if (code == LV_EVENT_RELEASED) {
        if (!UiManager::isLfoMode) {
            uint8_t note = UiManager::isMixerMode ? MidiManager::channelButtonNotes[MidiManager::currentMidiChannel] : MidiManager::mixerButtonNote;
            Control_Surface.sendNoteOff({note, (Channel)(MidiManager::channelButtonTargetChannel + 1)}, 0);
        }
    } else if (code == LV_EVENT_CLICKED) { 
        if (UiManager::isShiftActive) { UiManager::setMixerPage(0); return; }
        if (UiManager::isLfoMode) {
            LfoEngine::mixMode = true;
            UiManager::syncLfoArcValues();
        } else {
            UiManager::toggleMixerMode();
        }
        UiManager::updateLFOButtonColors(); 
    } 
}

void ui_event_ButtonWifi(lv_event_t *e) {
    if (lv_event_get_code(e) == LV_EVENT_CLICKED) {
        if (!WebServerManager::isWiFiEnabled()) WebServerManager::startWiFi();
        else WebServerManager::stopWiFi();
    }
}

// Delegate new menu system events to UiManager
extern void ui_event_MainMenuGeneric(lv_event_t *e);
extern void ui_event_TrackLayerGeneric(lv_event_t *e);
extern void ui_event_TrackTopGeneric(lv_event_t *e);
extern void ui_event_KeyboardGeneric(lv_event_t *e);