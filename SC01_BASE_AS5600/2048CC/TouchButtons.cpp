#include "TouchButtons.h"
#include "MidiManager.h"
#include "UiManager.h"
#include "LfoEngine.h"
#include "SensorManager.h"

// Static member definitions
int TouchButtons::buttonPins[2];
bool TouchButtons::buttonStates[2];
bool TouchButtons::buttonDetected[2];
void (*TouchButtons::buttonCallbacks[2])(int, bool);
void (*TouchButtons::interruptHandlers[2])();

void TouchButtons::handleTouch(int pin, bool isTouched) {
    static unsigned long pressStartTime = 0;

    if (pin == 12) {
        if (UiManager::currentMenuState == UiManager::MENU_TEMPLATES) {
            if (isTouched) {
                MidiManager::sendNoteOn(MidiManager::templatePin12Note, MidiManager::currentMidiChannel);
            } else {
                MidiManager::sendNoteOff(MidiManager::templatePin12Note, MidiManager::currentMidiChannel);
            }
            return;
        }

        UiManager::isShiftActive = isTouched;

        if (isTouched) {
            pressStartTime = millis();
            UiManager::shiftWasUsed = false;
            UiManager::previousMixerMode = UiManager::isMixerMode;
            UiManager::isMixerMode = true;
            UiManager::loadValuesForCurrentState();
            UiManager::updateParameterLabels();
            UiManager::updateLFOButtonColors();
        } else {
            unsigned long duration = millis() - pressStartTime;
            if (duration < 300) {
                UiManager::isMixerMode = false;
                UiManager::loadValuesForCurrentState();
                UiManager::updateParameterLabels();
                UiManager::updateLFOButtonColors();
            } else {
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

    if (pin == 14) {
        if (UiManager::currentMenuState == UiManager::MENU_TEMPLATES) {
            if (isTouched) {
                MidiManager::sendNoteOn(MidiManager::templatePin14Note, MidiManager::currentMidiChannel);
            } else {
                MidiManager::sendNoteOff(MidiManager::templatePin14Note, MidiManager::currentMidiChannel);
            }
            return;
        }

        if (isTouched) {
            UiManager::toggleLfoMode();
        }
    }
}
