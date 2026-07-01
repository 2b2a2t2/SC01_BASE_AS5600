#include "SensorManager.h"
#include "HardwareHAL.h"
#include "MidiManager.h"
#include "LfoEngine.h"
#include "UiManager.h"

// Static definitions
float SensorManager::previousAngles[20] = { 0 };
float SensorManager::potentiometerValues[20] = { 0 };
int SensorManager::currentMidiCCValues[NUM_POTS] = { 0 };
float SensorManager::currentPotentiometerValues[NUM_POTS] = { 0 };

void SensorManager::init() {
    for (int i = 0; i < 20; i++) {
        previousAngles[i] = 0;
        potentiometerValues[i] = 0;
    }
}

void SensorManager::update() {
    static unsigned long lastBatchTime = 0;
    unsigned long now = millis();
    if (now - lastBatchTime < 5) return;
    lastBatchTime = now;

    for (int i = 0; i < 20; i++) {
        uint8_t muxAddr;
        uint8_t channel;

        if (i < 8) { muxAddr = TCA9548A_ADDR_1; channel = MUX_PORTS_TCA1[i]; }
        else if (i < 16) { muxAddr = TCA9548A_ADDR_2; channel = MUX_PORTS_TCA2[i - 8]; }
        else { muxAddr = TCA9548A_ADDR_3; channel = MUX_PORTS_TCA3[i - 16]; }

        HardwareHAL::tcaSelect(muxAddr, channel);
        float currentAngle = HardwareHAL::readAngle();

        if (MidiManager::needsResync) {
            previousAngles[i] = currentAngle;
            if (i == 19) MidiManager::needsResync = false;
            continue;
        }

        // Check if an external MIDI update recently happened for this pot
        if (millis() - MidiManager::lastExternalUpdate[i] < 100) {
            previousAngles[i] = currentAngle;
            continue;
        }

        float angleChange = currentAngle - previousAngles[i];
        if (angleChange > 180) angleChange -= 360;
        else if (angleChange < -180) angleChange += 360;

        if (abs(angleChange) >= 0.2f) {
            if (i < 16) {
                if (UiManager::isMixerMode) {
                    updateMixerPot(i, currentAngle, angleChange);
                } else {
                    updateStandardPot(i, currentAngle, angleChange);
                }
            } else {
                if (UiManager::isLfoMode) {
                    updateLfoPot(i, currentAngle, angleChange);
                } else {
                    updateRowCCPot(i, currentAngle, angleChange);
                }
            }
            previousAngles[i] = currentAngle;
        }
    }
}

void SensorManager::updateStandardPot(int i, float currentAngle, float angleChange) {
    float sensitivity = 1.0f;
    uint8_t mode = MidiManager::storedPotentiometerModes[MidiManager::currentPage][MidiManager::currentMidiChannel][i];
    bool hasDetent = MidiManager::storedPotentiometerDetents[MidiManager::currentPage][MidiManager::currentMidiChannel][i];
    
    if (hasDetent) {
        sensitivity = 0.4f; // Reduced sensitivity for fine tuning around center
    }
    
    float scaledChange = (angleChange / 360.0) * 127.0f * sensitivity;
    
    if (mode == 0) {
        // Absolute mode
        potentiometerValues[i] += scaledChange;
        potentiometerValues[i] = constrain(potentiometerValues[i], 0, 127);
        int midiCCValue = (int)potentiometerValues[i];
        
        if (hasDetent) {
            if (midiCCValue >= LFO_OFFSET_DETENT_MIN && midiCCValue <= LFO_OFFSET_DETENT_MAX) {
                midiCCValue = 64;
                potentiometerValues[i] = 64.0f;
            }
        }
        
        // Smooth UI Update
        currentPotentiometerValues[i] = potentiometerValues[i];
        if (!UiManager::isMixerMode) UiManager::refreshSinglePot(i);
        
        // MIDI and Global Sync
        if (midiCCValue != currentMidiCCValues[i]) {
            MidiManager::updateGlobalValueSync(MidiManager::currentCCBase + i + 1, MidiManager::currentMidiChannel, midiCCValue, i);
            MidiManager::sendCC(i, midiCCValue);
            currentMidiCCValues[i] = midiCCValue;
        }
    } else {
        // Relative Mode (1 or 2) - Endless rotation
        static float relativeAccumulators[20] = {0};
        relativeAccumulators[i] += scaledChange;
        
        int steps = (int)relativeAccumulators[i];
        if (steps != 0) {
            relativeAccumulators[i] -= steps;
            
            if (mode == 1) { // 7Fh/01h
                // Map positive steps to 1..64, negative steps to 127..65
                // Wait, if steps=1 -> 1. If steps=-1 -> 127.
                int relVal = (steps > 0) ? steps : (128 + steps);
                MidiManager::sendCC(i, relVal);
            } else if (mode == 2) { // 3Fh/41h
                // Map positive steps to 65+, negative steps to 63-
                int relVal = 64 + steps;
                MidiManager::sendCC(i, relVal);
            }
        }
        
        // Continuous wrapping arc for visual feedback
        potentiometerValues[i] += scaledChange;
        while (potentiometerValues[i] > 127) potentiometerValues[i] -= 128.0f;
        while (potentiometerValues[i] < 0) potentiometerValues[i] += 128.0f;
        
        currentPotentiometerValues[i] = potentiometerValues[i];
        if (!UiManager::isMixerMode) UiManager::refreshSinglePot(i);
    }
}

void SensorManager::updateLfoPot(int i, float currentAngle, float angleChange) {
    float sensitivity = 1.0f;
    if (!LfoEngine::mixMode && (i == 16 || i == 19)) {
        sensitivity = 0.4f; // Decreased sensitivity for depth and offset in LFO mode
    }
    float scaledChange = (angleChange / 360.0) * 127.0f * sensitivity;
    potentiometerValues[i] += scaledChange;
    potentiometerValues[i] = constrain(potentiometerValues[i], 0, 127);
    int midiCCValue = (int)potentiometerValues[i];

    if (LfoEngine::mixMode) {
        if (i == 16) LfoEngine::mixAmounts[0] = midiCCValue / 127.0;
        if (i == 17) LfoEngine::mixAmounts[1] = midiCCValue / 127.0;
        if (i == 18) LfoEngine::mixAmounts[2] = midiCCValue / 127.0;
        if (i == 19) LfoEngine::mixAmounts[3] = midiCCValue / 127.0;
    } else {
        auto& lfo = LfoEngine::lfos[LfoEngine::currentLfoIndex];
        if (i == 16) lfo.depth = midiCCValue / 127.0;
        if (i == 17) lfo.rate = (midiCCValue / 127.0) * MAX_LFO_RATE;
        if (i == 18) {
            static float shapeAccumulator = 0;
            shapeAccumulator += angleChange;
            if (abs(shapeAccumulator) >= 60) {
                lfo.shape += (shapeAccumulator > 0 ? 1 : -1);
                lfo.shape = constrain(lfo.shape, 0, 6);
                shapeAccumulator = 0;
            }
        }
        if (i == 19) {
            // Apply center detent / snap around 50% (CC value 64)
            if (midiCCValue >= LFO_OFFSET_DETENT_MIN && midiCCValue <= LFO_OFFSET_DETENT_MAX) {
                midiCCValue = 64;
                potentiometerValues[i] = 64.0f;
            }
            lfo.offset = midiCCValue / 127.0;
        }
    }
    UiManager::refreshSinglePot(i);
}

void SensorManager::updateMixerPot(int i, float currentAngle, float angleChange) {
    int mixerPage = UiManager::currentMixerPage;
    uint8_t cc = MidiManager::mixerPageCCs[mixerPage][i];
    uint8_t channel = MidiManager::mixerPageChannels[mixerPage][i];

    int detentPage = (cc - 1) / 16;
    int detentPot = (cc - 1) % 16;
    bool hasDetent = MidiManager::storedPotentiometerDetents[detentPage][channel][detentPot];

    float sensitivity = hasDetent ? 0.4f : 1.0f;
    float scaledChange = (angleChange / 360.0) * 127.0f * sensitivity;

    MidiManager::mixerPageValues[mixerPage][i] += scaledChange;
    MidiManager::mixerPageValues[mixerPage][i] = constrain(MidiManager::mixerPageValues[mixerPage][i], 0, 127);
    int midiVal = (int)MidiManager::mixerPageValues[mixerPage][i];

    if (hasDetent && midiVal >= LFO_OFFSET_DETENT_MIN && midiVal <= LFO_OFFSET_DETENT_MAX) {
        midiVal = 64;
        MidiManager::mixerPageValues[mixerPage][i] = 64.0f;
    }

    // Smooth UI Update (Every tick)
    if (UiManager::isMixerMode) {
        currentPotentiometerValues[i] = MidiManager::mixerPageValues[mixerPage][i];
        UiManager::refreshSinglePot(i);
    }

    // MIDI and Global Sync (Only on integer change)
    if (midiVal != currentMidiCCValues[i]) {
        currentMidiCCValues[i] = midiVal;
        
        MidiManager::ignoreIncomingMIDI = true;
        Control_Surface.sendCC({ cc, MidiManager::getMIDIChannel(channel) }, midiVal);
        MidiManager::ignoreIncomingMIDI = false;

        MidiManager::updateGlobalValueSync(cc, channel, midiVal, i);
    }
}

void SensorManager::updateRowCCPot(int i, float currentAngle, float angleChange) {
    float scaledChange = (angleChange / 360.0) * 127.0f;
    potentiometerValues[i] += scaledChange;
    potentiometerValues[i] = constrain(potentiometerValues[i], 0, 127);
    int midiVal = (int)potentiometerValues[i];

    // Smooth UI Update (Every tick)
    currentPotentiometerValues[i] = potentiometerValues[i];
    UiManager::refreshSinglePot(i);

    // MIDI and Global Sync (Only on integer change)
    if (midiVal != currentMidiCCValues[i]) {
        currentMidiCCValues[i] = midiVal;
        
        int rowPotIdx = i - 16;
        MidiManager::rowPotentiometerValues[UiManager::ccRowIndex][rowPotIdx] = potentiometerValues[i];

        int ccNumber = (UiManager::ccRowIndex * 4) + rowPotIdx + 1;
        MidiManager::ignoreIncomingMIDI = true;
        Control_Surface.sendCC({ (uint8_t)ccNumber, MidiManager::getMIDIChannel(MidiManager::currentMidiChannel) }, midiVal);
        MidiManager::ignoreIncomingMIDI = false;

        MidiManager::updateGlobalValueSync(ccNumber, MidiManager::currentMidiChannel, midiVal, i);
    }
}


float SensorManager::getPotValue(int index) {
    if (index >= 0 && index < 20) return potentiometerValues[index];
    return 0;
}

int SensorManager::getMidiCCValue(int index) {
    if (index >= 0 && index < NUM_POTS) return currentMidiCCValues[index];
    return 0;
}
