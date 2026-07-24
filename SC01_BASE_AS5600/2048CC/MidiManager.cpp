#include "MidiManager.h"
#include "UiManager.h"
#include "SensorManager.h"
#include "LfoEngine.h"
#include "StorageManager.h"

BluetoothMIDI_Interface midi_ble;

// Static member definitions
MidiManager::Callbacks MidiManager::midi_callbacks;
int MidiManager::currentPage = 0;
int MidiManager::currentMidiChannel = 4;
int MidiManager::currentCCBase = 0;
int MidiManager::channelPages[NUM_CHANNELS] = { 0 };
int MidiManager::channelRows[NUM_CHANNELS] = { 0 };

int MidiManager::activeTrackLayer = 0;
int MidiManager::keyboardOctave = 0;
TrackActionButtonConfig MidiManager::trackActions[4][4];
String MidiManager::trackLayerLabels[4] = { "Transport", "Pattern", "Perform", "Sequencer" };
TrackActionButtonConfig MidiManager::trackLayerActions[4];

int MidiManager::storedMidiCCValues[NUM_PAGES][NUM_CHANNELS][NUM_POTS];
float MidiManager::storedPotentiometerValues[NUM_PAGES][NUM_CHANNELS][NUM_POTS];
uint8_t MidiManager::storedPotentiometerModes[NUM_PAGES][NUM_CHANNELS][NUM_POTS];
bool MidiManager::storedPotentiometerDetents[NUM_PAGES][NUM_CHANNELS][NUM_POTS];
float MidiManager::mixerVolumes[16] = { 0 };
float MidiManager::rowPotentiometerValues[4][4] = { 0 };
String MidiManager::currentArcLabels[NUM_POTS];
String MidiManager::currentMixerArcLabels[16];
bool MidiManager::ignoreIncomingMIDI = false;
unsigned long MidiManager::lastExternalUpdate[20] = { 0 };
unsigned long MidiManager::lastLocalUpdate[20] = { 0 };
bool MidiManager::needsResync = false;

int MidiManager::selectedArcForModulation = 1;
int MidiManager::modulationTargetPage = 0;
int MidiManager::modulationTargetChannel = 4;
bool MidiManager::modulationEnabled = true;

uint8_t MidiManager::channelButtonNotes[16] = {59, 60, 61, 62, 63, 64, 65, 66, 67, 68, 69, 70, 71, 72, 73, 74};
uint8_t MidiManager::channelButtonTargetChannel = 0;
uint8_t MidiManager::mixerButtonNote = 75;

float MidiManager::mixerPageValues[5][16] = { 0 };
uint8_t MidiManager::mixerPageCCs[5][16] = { 0 };
uint8_t MidiManager::mixerPageChannels[5][16] = { 0 };
String MidiManager::mixerButtonLabels[5] = { "Mixer", "Pan", "Rev", "Cho", "Mod" };

uint8_t MidiManager::templateNotes[8] = { 60, 62, 64, 65, 67, 69, 71, 72 };
String MidiManager::templateLabels[8] = { "Note 1", "Note 2", "Note 3", "Note 4", "Note 5", "Note 6", "Note 7", "Note 8" };
uint8_t MidiManager::templatePin12Note = 74;
uint8_t MidiManager::templatePin14Note = 76;
uint8_t MidiManager::templateArcNotes[16] = { 36, 37, 38, 39, 40, 41, 42, 43, 44, 45, 46, 47, 48, 49, 50, 51 };

void MidiManager::init() {
    midi_ble.setName("BAT LiL");
    midi_ble.setAsDefault();
    Control_Surface.begin();
    midi_ble.setCallbacks(midi_callbacks);

    // Initialize Mixer Page 0 (Default Mixer)
    for (int i = 0; i < 16; i++) {
        mixerPageCCs[0][i] = 7;
        mixerPageChannels[0][i] = i;
    }
    
    // Initialize Mixer Pages 1-4 (Custom Pages)
    uint8_t defaultCCs[] = { 10, 91, 93, 1 };
    String defaultBaseLabels[] = { "Pan", "Rev", "Cho", "Mod" };
    for (int p = 1; p < 5; p++) {
        for (int i = 0; i < 16; i++) {
            mixerPageCCs[p][i] = defaultCCs[p - 1];
            mixerPageChannels[p][i] = i; // Default to channels 1-16
        }
    }

    // Initialize Track Actions Defaults
    String layerLabels[4] = { "Transport", "Pattern", "Perform", "Sequencer" };
    String buttonLabels[4][4] = {
        { "Play", "Stop", "Rec", "Tap" },
        { "Pat 1", "Pat 2", "Pat 3", "Pat 4" },
        { "Mute", "Solo", "Clear", "Undo" },
        { "Prev", "Next", "Copy", "Paste" }
    };

    for (int i = 0; i < 4; i++) {
        trackLayerLabels[i] = layerLabels[i];
        trackLayerActions[i].type = 0; // Default to None (type = 0)
        trackLayerActions[i].value = 24 + i; // Default Note values
        trackLayerActions[i].channel = 0;
        trackLayerActions[i].label = layerLabels[i];
    }

    for (int l = 0; l < 4; l++) {
        for (int b = 0; b < 4; b++) {
            trackActions[l][b].type = 1; // Default to Note (type = 1)
            trackActions[l][b].value = 36 + (l * 4) + b; // Default MIDI notes
            trackActions[l][b].channel = 0;
            trackActions[l][b].label = buttonLabels[l][b];
        }
    }

    currentCCBase = currentPage * 16;
    for (int i = 0; i < NUM_CHANNELS; i++) {
        channelPages[i] = 0; // Default all channels to page 1
        channelRows[i] = 0;  // Default all channels to row 1 (index 0)
    }

    for (int page = 0; page < NUM_PAGES; page++) {
        for (int ch = 0; ch < NUM_CHANNELS; ch++) {
            for (int pot = 0; pot < NUM_POTS; pot++) {
                storedPotentiometerModes[page][ch][pot] = 0;
                storedPotentiometerDetents[page][ch][pot] = false;
            }
        }
    }
}

void MidiManager::update() {
    midi_ble.update();
}

Channel MidiManager::getMIDIChannel(int channelIndex) {
    switch (channelIndex) {
        case 0: return CHANNEL_1; case 1: return CHANNEL_2; case 2: return CHANNEL_3; case 3: return CHANNEL_4;
        case 4: return CHANNEL_5; case 5: return CHANNEL_6; case 6: return CHANNEL_7; case 7: return CHANNEL_8;
        case 8: return CHANNEL_9; case 9: return CHANNEL_10; case 10: return CHANNEL_11; case 11: return CHANNEL_12;
        case 12: return CHANNEL_13; case 13: return CHANNEL_14; case 14: return CHANNEL_15; case 15: return CHANNEL_16;
        default: return CHANNEL_1;
    }
}

void MidiManager::sendCC(int potIndex, int value) {
    ignoreIncomingMIDI = true;
    int ccNumber = currentCCBase + potIndex + 1;
    MIDIAddress ccAddress = { (uint8_t)ccNumber, getMIDIChannel(currentMidiChannel) };
    Control_Surface.sendCC(ccAddress, value);
    ignoreIncomingMIDI = false;
}

void MidiManager::sendModulatedCC(int arcIndex, int value, int targetPage, int targetChannel) {
    ignoreIncomingMIDI = true;
    int targetCCBase = targetPage * 16;
    int ccNumber = targetCCBase + arcIndex + 1;
    MIDIAddress ccAddress = { (uint8_t)ccNumber, getMIDIChannel(targetChannel) };
    Control_Surface.sendCC(ccAddress, value);
    ignoreIncomingMIDI = false;
}

void MidiManager::updateModulation(float mixedValue) {
    int arcIndex = selectedArcForModulation - 1;
    if (arcIndex < 0 || arcIndex >= 16) return;

    bool hasActiveModulation = modulationEnabled && !LfoEngine::isKilled && LfoEngine::anyMixActive;

    static int lastSentModulatedValue[16] = { -1 };
    static bool wasModulating[16] = { false };

    if (hasActiveModulation) {
        int baseValue = storedMidiCCValues[modulationTargetPage][modulationTargetChannel][arcIndex];
        float offset = (mixedValue - 0.5f) * 127.0f;
        int finalMidiValue = constrain((int)(baseValue + offset), 0, 127);

        if (finalMidiValue != lastSentModulatedValue[arcIndex]) {
            sendModulatedCC(arcIndex, finalMidiValue, modulationTargetPage, modulationTargetChannel);
            lastSentModulatedValue[arcIndex] = finalMidiValue;
        }
        wasModulating[arcIndex] = true;
    } else {
        if (wasModulating[arcIndex]) {
            int baseValue = storedMidiCCValues[modulationTargetPage][modulationTargetChannel][arcIndex];
            sendModulatedCC(arcIndex, baseValue, modulationTargetPage, modulationTargetChannel);
            lastSentModulatedValue[arcIndex] = baseValue;
            wasModulating[arcIndex] = false;
        }
    }
}

uint8_t MidiManager::getVelocityFromCurve(uint8_t rawVelocity) {
    switch (UiManager::velocityCurve) {
        case 0: return rawVelocity;                         // Linear
        case 1: return (rawVelocity * rawVelocity) / 127;    // Soft (exponential)
        case 2: return 127 - ((127 - rawVelocity) * (127 - rawVelocity)) / 127; // Hard
        case 3: return 100;                                   // Fixed
        default: return rawVelocity;
    }
}

void MidiManager::sendNoteOn(uint8_t note, uint8_t channel, uint8_t velocity) {
    uint8_t vel = getVelocityFromCurve(velocity);
    Control_Surface.sendNoteOn({note, getMIDIChannel(channel)}, vel);
}

void MidiManager::sendNoteOff(uint8_t note, uint8_t channel) {
    Control_Surface.sendNoteOff({note, getMIDIChannel(channel)}, 0);
}

void MidiManager::sendTrackActionPress(int buttonIndex) {
    if (buttonIndex < 0 || buttonIndex >= 4) return;
    TrackActionButtonConfig &config = trackActions[activeTrackLayer][buttonIndex];
    if (config.type == 1) { // Note
        sendNoteOn(config.value, config.channel);
    } else if (config.type == 2) { // CC
        MIDIAddress ccAddress = { config.value, getMIDIChannel(config.channel) };
        Control_Surface.sendCC(ccAddress, 127);
    }
}

void MidiManager::sendTrackActionRelease(int buttonIndex) {
    if (buttonIndex < 0 || buttonIndex >= 4) return;
    TrackActionButtonConfig &config = trackActions[activeTrackLayer][buttonIndex];
    if (config.type == 1) { // Note
        sendNoteOff(config.value, config.channel);
    } else if (config.type == 2) { // CC
        MIDIAddress ccAddress = { config.value, getMIDIChannel(config.channel) };
        Control_Surface.sendCC(ccAddress, 0);
    }
}

void MidiManager::sendTrackLayerPress(int layerIndex) {
    if (layerIndex < 0 || layerIndex >= 4) return;
    TrackActionButtonConfig &config = trackLayerActions[layerIndex];
    if (config.type == 1) { // Note
        sendNoteOn(config.value, config.channel);
    } else if (config.type == 2) { // CC
        MIDIAddress ccAddress = { config.value, getMIDIChannel(config.channel) };
        Control_Surface.sendCC(ccAddress, 127);
    }
}

void MidiManager::sendTrackLayerRelease(int layerIndex) {
    if (layerIndex < 0 || layerIndex >= 4) return;
    TrackActionButtonConfig &config = trackLayerActions[layerIndex];
    if (config.type == 1) { // Note
        sendNoteOff(config.value, config.channel);
    } else if (config.type == 2) { // CC
        MIDIAddress ccAddress = { config.value, getMIDIChannel(config.channel) };
        Control_Surface.sendCC(ccAddress, 0);
    }
}

void MidiManager::handleIncomingMIDI(Channel channel, uint8_t controller, uint8_t value) {
    if (ignoreIncomingMIDI) return;

    int targetChannel = channel.getRaw(); // getRaw returns 0-15
    if (targetChannel < 0 || targetChannel >= 16) return;

    // Ignore incoming MIDI for the modulated CC if modulation is active and there is non-zero LFO mix
    if (modulationEnabled && !LfoEngine::isKilled && selectedArcForModulation >= 1 && LfoEngine::anyMixActive) {
        int modulatedArcIdx = selectedArcForModulation - 1;
        int modulatedCC = (modulationTargetPage * 16) + modulatedArcIdx + 1;
        if (controller == modulatedCC && targetChannel == modulationTargetChannel) {
            return;
        }
    }

    // 1. Prevent feedback loops: ignore incoming MIDI if user is actively turning the local physical encoder
    if (controller >= 1 && controller <= 128) {
        int targetPage = (controller - 1) / 16;
        int targetPot = (controller - 1) % 16;
        
        // Ignore if value is already identical
        if (storedMidiCCValues[targetPage][targetChannel][targetPot] == value) {
            return;
        }

        // If this pot is currently active on screen
        if (targetPage == currentPage && targetChannel == currentMidiChannel) {
            if (millis() - lastLocalUpdate[targetPot] < 300) {
                return; // Ignore incoming message as user is actively turning this encoder
            }
        }

        // Check mirrored row pots (16-19)
        if (!UiManager::isLfoMode) {
            int rowStartCC = UiManager::ccRowIndex * 4;
            int rowPage = rowStartCC / 16;
            int rowPotStart = rowStartCC % 16;
            if (targetPage == rowPage && targetChannel == currentMidiChannel) {
                if (targetPot >= rowPotStart && targetPot < rowPotStart + 4) {
                    int rowArcIdx = 16 + (targetPot - rowPotStart);
                    if (millis() - lastLocalUpdate[rowArcIdx] < 300) {
                        return;
                    }
                }
            }
        }
    }

    // 2. Mixer mode feedback loop prevention
    for (int p = 0; p < 5; p++) {
        for (int i = 0; i < 16; i++) {
            if (mixerPageCCs[p][i] == controller && mixerPageChannels[p][i] == targetChannel) {
                // Ignore if value is identical
                if ((int)mixerPageValues[p][i] == value) {
                    return;
                }
                
                if (UiManager::isMixerMode && UiManager::currentMixerPage == p) {
                    if (millis() - lastLocalUpdate[i] < 300) {
                        return;
                    }
                }
            }
        }
    }

    updateGlobalValueSync(controller, targetChannel, value);
}

void MidiManager::updateGlobalValueSync(uint8_t cc, uint8_t channel, int value, int initiatorIndex) {
    if (initiatorIndex != -1 && initiatorIndex >= 0 && initiatorIndex < 20) {
        lastLocalUpdate[initiatorIndex] = millis();
    }

    // 1. Update standard storage
    if (cc >= 1 && cc <= 128) {
        int targetPage = (cc - 1) / 16;
        int targetPot = (cc - 1) % 16;
        storedMidiCCValues[targetPage][channel][targetPot] = value;
        storedPotentiometerValues[targetPage][channel][targetPot] = (float)value;

        // If this is live on screen (Standard Mode or Row Mirror)
        if (targetPage == currentPage && channel == currentMidiChannel) {
            SensorManager::currentMidiCCValues[targetPot] = value;
            SensorManager::currentPotentiometerValues[targetPot] = (float)value;
            SensorManager::potentiometerValues[targetPot] = (float)value;
            if (initiatorIndex != targetPot) lastExternalUpdate[targetPot] = millis();
            
            // Refresh the standard 16 arcs only if we're not in Mixer mode
            if (!UiManager::isMixerMode) UiManager::refreshSinglePot(targetPot);
        }

        // Handle Mirror Sync (Arcs 17-20) - These mirror the active Row (ccRowIndex)
        // ONLY propagate to live arcs if NOT in LFO mode
        if (!UiManager::isLfoMode) {
            int rowStartCC = UiManager::ccRowIndex * 4;
            int rowPage = rowStartCC / 16;
            int rowPotStart = rowStartCC % 16;
            
            if (targetPage == rowPage && channel == currentMidiChannel) {
                if (targetPot >= rowPotStart && targetPot < rowPotStart + 4) {
                    int rowArcIdx = 16 + (targetPot - rowPotStart);
                    SensorManager::currentMidiCCValues[rowArcIdx] = value;
                    SensorManager::currentPotentiometerValues[rowArcIdx] = (float)value;
                    SensorManager::potentiometerValues[rowArcIdx] = (float)value;
                    if (initiatorIndex != rowArcIdx) lastExternalUpdate[rowArcIdx] = millis();
                    UiManager::refreshSinglePot(rowArcIdx);
                }
            }
        }
    }

    // 2. Update mixer storage
    for (int p = 0; p < 5; p++) {
        for (int i = 0; i < 16; i++) {
            if (mixerPageCCs[p][i] == cc && mixerPageChannels[p][i] == channel) {
                mixerPageValues[p][i] = (float)value;
                // UI Refresh for Mixer Mode
                if (UiManager::isMixerMode && UiManager::currentMixerPage == p) {
                    SensorManager::currentMidiCCValues[i] = value;
                    SensorManager::currentPotentiometerValues[i] = (float)value;
                    SensorManager::potentiometerValues[i] = (float)value;
                    if (initiatorIndex != i) lastExternalUpdate[i] = millis();
                    UiManager::refreshSinglePot(i);
                }
            }
        }
    }
    
    // Debugging
    if (initiatorIndex != -1) {
        Serial.printf("Sync: CC %d, Ch %d -> Val %d [Src Arc %d]\n", cc, channel + 1, value, initiatorIndex + 1);
    }
}

void MidiManager::loadValuesForCurrentState() {
    StorageManager::loadLabelsForCurrentState();

    needsResync = true;

    if (UiManager::isMixerMode) {
        StorageManager::loadMixerLabelsForCurrentState();
        for (int i = 0; i < 16; i++) {
            float val = mixerPageValues[UiManager::currentMixerPage][i];
            SensorManager::potentiometerValues[i] = val;
            SensorManager::currentPotentiometerValues[i] = val;
            SensorManager::currentMidiCCValues[i] = (int)val;
        }
    } else {
        // Standard CC mode
        for (int i = 0; i < 16; i++) {
            SensorManager::currentPotentiometerValues[i] = storedPotentiometerValues[currentPage][currentMidiChannel][i];
            SensorManager::currentMidiCCValues[i] = storedMidiCCValues[currentPage][currentMidiChannel][i];
            SensorManager::potentiometerValues[i] = SensorManager::currentPotentiometerValues[i];
        }
    }
    
    if (!UiManager::isLfoMode) {
        // Load Arcs 17-20 from the selected Row of Page 1 (index 0)
        int startIdx = UiManager::ccRowIndex * 4;
        for (int i = 0; i < 4; i++) {
            float val = storedPotentiometerValues[0][currentMidiChannel][startIdx + i];
            SensorManager::potentiometerValues[16 + i] = val;
            SensorManager::currentPotentiometerValues[16 + i] = val;
            SensorManager::currentMidiCCValues[16 + i] = (int)val;
        }
    } else {
        // Load LFO values into potentiometerValues 16-19 so they are ready for updateLfoPot
        if (LfoEngine::mixMode) {
            SensorManager::potentiometerValues[16] = LfoEngine::mixAmounts[0] * 127.0;
            SensorManager::potentiometerValues[17] = LfoEngine::mixAmounts[1] * 127.0;
            SensorManager::potentiometerValues[18] = LfoEngine::mixAmounts[2] * 127.0;
            SensorManager::potentiometerValues[19] = LfoEngine::mixAmounts[3] * 127.0;
        } else {
            auto& lfo = LfoEngine::lfos[LfoEngine::currentLfoIndex];
            SensorManager::potentiometerValues[16] = lfo.depth * 127.0;
            SensorManager::potentiometerValues[17] = (lfo.rate / MAX_LFO_RATE) * 127.0;
            SensorManager::potentiometerValues[18] = (lfo.shape / 6.0) * 127.0;
            SensorManager::potentiometerValues[19] = lfo.offset * 127.0;
        }
    }
    
    needsResync = true;
    Serial.printf("MidiManager: State loaded (Page %d, Channel %d). Mode: %s\n", 
                  currentPage + 1, currentMidiChannel + 1, UiManager::isLfoMode ? "LFO" : "CC");
}

String MidiManager::getGlobalLabel(uint8_t cc, uint8_t channel) {
    if (cc < 1 || cc > 128) return "CC " + String(cc);
    
    int targetPage = (cc - 1) / 16;
    int targetPot = (cc - 1) % 16;
    String label = StorageManager::getLabel(targetPage, channel, targetPot);
    
    if (label.length() > 0 && !label.startsWith("CC ")) return label;
    return "CC " + String(cc);
}

String MidiManager::getMixerLabel(int page, int arcIndex) {
    if (page < 0 || page >= 5 || arcIndex < 0 || arcIndex >= 16) return "";
    return currentMixerArcLabels[arcIndex];
}

void MidiManager::setMidiChannel(int channelIndex) {
    if (channelIndex >= 0 && channelIndex < NUM_CHANNELS) {
        currentMidiChannel = channelIndex;
        // Recall the page for this channel
        currentPage = channelPages[currentMidiChannel];
        currentCCBase = currentPage * 16;
        // Recall the row for this channel
        UiManager::ccRowIndex = channelRows[currentMidiChannel];
        StorageManager::loadLabelsForCurrentState();
    }
}

void MidiManager::setPage(int pageIndex) {
    if (pageIndex >= 0 && pageIndex < NUM_PAGES) {
        currentPage = pageIndex;
        currentCCBase = currentPage * 16;
        // Store the page for the current channel
        channelPages[currentMidiChannel] = currentPage;
        StorageManager::loadLabelsForCurrentState();
    }
}

void MidiManager::sendTemplateActionPress(int index) {
    if (index >= 0 && index < 8) {
        sendNoteOn(templateNotes[index], currentMidiChannel);
        // If it's the second row (indices 4 to 7), also trigger CC page change (pages 5 to 8, which are index 4 to 7)
        if (index >= 4 && index <= 7) {
            setPage(index);
            UiManager::loadValuesForCurrentState();
            UiManager::updateParameterLabels();
            UiManager::updatePageButtonColors();
        }
    }
}

void MidiManager::sendTemplateActionRelease(int index) {
    if (index >= 0 && index < 8) {
        sendNoteOff(templateNotes[index], currentMidiChannel);
    }
}

void MidiManager::sendTemplateArcActionPress(int arcIndex) {
    if (arcIndex >= 0 && arcIndex < 16) {
        sendNoteOn(templateArcNotes[arcIndex], currentMidiChannel);
    }
}

void MidiManager::sendTemplateArcActionRelease(int arcIndex) {
    if (arcIndex >= 0 && arcIndex < 16) {
        sendNoteOff(templateArcNotes[arcIndex], currentMidiChannel);
    }
}
