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
                if (UiManager::currentMenuState == UiManager::MENU_KEYBOARD) {
                    updateKeyboardPot(i, currentAngle, angleChange);
                } else if (UiManager::isLfoMode) {
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

void SensorManager::updateKeyboardPot(int i, float currentAngle, float angleChange) {
    float scaledChange = (angleChange / 360.0) * 127.0f;
    potentiometerValues[i] += scaledChange;
    potentiometerValues[i] = constrain(potentiometerValues[i], 0, 127);
    int midiVal = (int)potentiometerValues[i];

    if (UiManager::keyboardSubmode == UiManager::SUBMODE_KEYS) {
        if (i == 16) {
            static float octAccum = 0;
            octAccum += angleChange;
            if (abs(octAccum) >= 40) {
                int delta = (octAccum > 0) ? 1 : -1;
                MidiManager::keyboardOctave += delta;
                MidiManager::keyboardOctave = constrain(MidiManager::keyboardOctave, -4, 4);
                octAccum = 0;
                UiManager::updateParameterLabels();
                UiManager::updateLFOButtonColors();
            }
        } else if (i == 17) {
            static float scaleAccum = 0;
            scaleAccum += angleChange;
            if (abs(scaleAccum) >= 40) {
                int delta = (scaleAccum > 0) ? 1 : -1;
                UiManager::selectedScale += delta;
                if (UiManager::selectedScale < 0) UiManager::selectedScale = UiManager::NUM_SCALES - 1;
                if (UiManager::selectedScale >= UiManager::NUM_SCALES) UiManager::selectedScale = 0;
                scaleAccum = 0;
                UiManager::updateParameterLabels();
                UiManager::updateKeyboardColors();
            }
        } else if (i == 18) {
            static float velAccum = 0;
            velAccum += angleChange;
            if (abs(velAccum) >= 40) {
                int delta = (velAccum > 0) ? 1 : -1;
                UiManager::velocityCurve += delta;
                UiManager::velocityCurve = constrain(UiManager::velocityCurve, 0, 3);
                velAccum = 0;
                UiManager::updateParameterLabels();
            }
        } else if (i == 19) {
            static int lastModVal = -1;
            if (abs(midiVal - lastModVal) >= 2) {
                UiManager::modWheelValue = midiVal;
                Control_Surface.sendCC({1, MidiManager::getMIDIChannel(MidiManager::currentMidiChannel)}, UiManager::modWheelValue);
                lastModVal = midiVal;
                UiManager::updateParameterLabels();
            }
        }
    } else if (UiManager::keyboardSubmode == UiManager::SUBMODE_CHORD) {
        if (i == 16) {
            static float octAccum = 0;
            octAccum += angleChange;
            if (abs(octAccum) >= 40) {
                int delta = (octAccum > 0) ? 1 : -1;
                UiManager::chordOctave += delta;
                UiManager::chordOctave = constrain(UiManager::chordOctave, -2, 2);
                octAccum = 0;
                UiManager::updateParameterLabels();
            }
        } else if (i == 17) {
            static float setAccum = 0;
            static unsigned long lastSetChangeTime = 0;
            static bool pendingChordSetLoad = false;
            setAccum += angleChange;
            if (abs(setAccum) >= 40) {
                int delta = (setAccum > 0) ? 1 : -1;
                UiManager::selectedChordSet += delta;
                if (UiManager::selectedChordSet < 0) UiManager::selectedChordSet = UiManager::NUM_CHORD_SETS - 1;
                if (UiManager::selectedChordSet >= UiManager::NUM_CHORD_SETS) UiManager::selectedChordSet = 0;
                setAccum = 0;
                UiManager::updateParameterLabels();
                lastSetChangeTime = millis();
                pendingChordSetLoad = true;
            }
            if (pendingChordSetLoad && (millis() - lastSetChangeTime >= 400)) {
                UiManager::loadChordSetNotes(UiManager::selectedChordSet);
                UiManager::updateKeyboardColors();
                pendingChordSetLoad = false;
            }
        }
    } else if (UiManager::keyboardSubmode == UiManager::SUBMODE_CHORD_TYPE) {
        // Pots: 16=octave, 17=type, 18=inversion, 19=voicing
        if (i == 16) {
            static float octAccum = 0;
            octAccum += angleChange;
            if (abs(octAccum) >= 40) {
                int delta = (octAccum > 0) ? 1 : -1;
                UiManager::chordOctave += delta;
                UiManager::chordOctave = constrain(UiManager::chordOctave, -2, 2);
                octAccum = 0;
                UiManager::updateParameterLabels();
            }
        } else if (i == 17) {
            static float typeAccum = 0;
            typeAccum += angleChange;
            if (abs(typeAccum) >= 40) {
                int delta = (typeAccum > 0) ? 1 : -1;
                UiManager::selectedChordType += delta;
                if (UiManager::selectedChordType < 0) UiManager::selectedChordType = UiManager::NUM_CHORD_TYPES - 1;
                if (UiManager::selectedChordType >= UiManager::NUM_CHORD_TYPES) UiManager::selectedChordType = 0;
                typeAccum = 0;
                UiManager::updateParameterLabels();
                UiManager::updateKeyboardColors();
            }
        } else if (i == 18) {
            static float invAccum = 0;
            invAccum += angleChange;
            if (abs(invAccum) >= 40) {
                int delta = (invAccum > 0) ? 1 : -1;
                UiManager::selectedInversion += delta;
                if (UiManager::selectedInversion < 0) UiManager::selectedInversion = UiManager::NUM_INVERSIONS - 1;
                if (UiManager::selectedInversion >= UiManager::NUM_INVERSIONS) UiManager::selectedInversion = 0;
                invAccum = 0;
                UiManager::updateParameterLabels();
            }
        } else if (i == 19) {
            static float voiceAccum = 0;
            voiceAccum += angleChange;
            if (abs(voiceAccum) >= 40) {
                int delta = (voiceAccum > 0) ? 1 : -1;
                UiManager::selectedVoicing += delta;
                if (UiManager::selectedVoicing < 0) UiManager::selectedVoicing = UiManager::NUM_VOICINGS - 1;
                if (UiManager::selectedVoicing >= UiManager::NUM_VOICINGS) UiManager::selectedVoicing = 0;
                voiceAccum = 0;
                UiManager::updateParameterLabels();
            }
        }
    } else if (UiManager::keyboardSubmode == UiManager::SUBMODE_PROGRESSION) {
        // Pots: 16=octave, 17=progression, 18=root key, 19=inversion
        if (i == 16) {
            static float octAccum = 0;
            octAccum += angleChange;
            if (abs(octAccum) >= 40) {
                int delta = (octAccum > 0) ? 1 : -1;
                UiManager::chordOctave += delta;
                UiManager::chordOctave = constrain(UiManager::chordOctave, -2, 2);
                octAccum = 0;
                UiManager::updateParameterLabels();
            }
        } else if (i == 17) {
            static float progAccum = 0;
            progAccum += angleChange;
            if (abs(progAccum) >= 40) {
                int delta = (progAccum > 0) ? 1 : -1;
                UiManager::selectedProgression += delta;
                if (UiManager::selectedProgression < 0) UiManager::selectedProgression = UiManager::NUM_PROGRESSIONS - 1;
                if (UiManager::selectedProgression >= UiManager::NUM_PROGRESSIONS) UiManager::selectedProgression = 0;
                progAccum = 0;
                UiManager::progressionStep = 0;
                UiManager::updateParameterLabels();
                UiManager::updateKeyboardColors();
            }
        } else if (i == 18) {
            static float rootAccum = 0;
            rootAccum += angleChange;
            if (abs(rootAccum) >= 40) {
                int delta = (rootAccum > 0) ? 1 : -1;
                UiManager::progressionRoot += delta;
                if (UiManager::progressionRoot < 0) UiManager::progressionRoot = 11;
                if (UiManager::progressionRoot > 11) UiManager::progressionRoot = 0;
                rootAccum = 0;
                UiManager::progressionStep = 0;
                UiManager::updateParameterLabels();
                UiManager::updateKeyboardColors();
            }
        } else if (i == 19) {
            static float invAccum = 0;
            invAccum += angleChange;
            if (abs(invAccum) >= 40) {
                int delta = (invAccum > 0) ? 1 : -1;
                UiManager::selectedInversion += delta;
                if (UiManager::selectedInversion < 0) UiManager::selectedInversion = UiManager::NUM_INVERSIONS - 1;
                if (UiManager::selectedInversion >= UiManager::NUM_INVERSIONS) UiManager::selectedInversion = 0;
                invAccum = 0;
                UiManager::updateParameterLabels();
            }
        }
    }

    currentPotentiometerValues[i] = potentiometerValues[i];
    UiManager::refreshSinglePot(i);
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
