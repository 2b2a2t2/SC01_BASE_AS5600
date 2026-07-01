#ifndef MIDI_MANAGER_H
#define MIDI_MANAGER_H

#include <Arduino.h>
#include <Control_Surface.h>
#include "Config.h"

// BLE MIDI interface — declared here so init/update live inside MidiManager
extern BluetoothMIDI_Interface midi_ble;

struct TrackActionButtonConfig {
    uint8_t type; // 0 = none, 1 = note, 2 = CC
    uint8_t value; // note or CC number
    uint8_t channel; // 0-15
    String label;
};

class MidiManager {
public:
    static void init();
    static void update();
    
    // MIDI state
    static int currentPage;
    static int currentMidiChannel;
    static int currentCCBase;
    static int channelPages[NUM_CHANNELS];
    static int channelRows[NUM_CHANNELS];
    static uint8_t storedPotentiometerModes[NUM_PAGES][NUM_CHANNELS][NUM_POTS];
    static bool storedPotentiometerDetents[NUM_PAGES][NUM_CHANNELS][NUM_POTS];
    
    // Menu & Keyboard state
    static int activeTrackLayer;
    static int keyboardOctave;
    static TrackActionButtonConfig trackActions[4][4]; // 4 layers x 4 buttons
    static String trackLayerLabels[4];
    static TrackActionButtonConfig trackLayerActions[4]; // 4 layer selector buttons
    
    // Template state
    static uint8_t templateNotes[8];
    static String templateLabels[8];
    static uint8_t templatePin12Note;
    static uint8_t templatePin14Note;
    static uint8_t templateArcNotes[16];
    
    // Data storage
    static int storedMidiCCValues[NUM_PAGES][NUM_CHANNELS][NUM_POTS];
    static float storedPotentiometerValues[NUM_PAGES][NUM_CHANNELS][NUM_POTS];
    static float mixerVolumes[16];
    static float rowPotentiometerValues[4][4]; // 4 rows x 4 pots
    static String currentArcLabels[NUM_POTS]; // Only labels for active page/channel
    static String currentMixerArcLabels[16];
    static uint8_t channelButtonNotes[16];
    static uint8_t channelButtonTargetChannel;
    static uint8_t mixerButtonNote;
    
    static float mixerPageValues[5][16];
    static uint8_t mixerPageCCs[5][16];
    static uint8_t mixerPageChannels[5][16];
    static String mixerButtonLabels[5];
    
    // MIDI Handlers
    static void handleIncomingMIDI(Channel channel, uint8_t controller, uint8_t value);
    static void setMidiChannel(int channelIndex);
    static void setPage(int pageIndex);
    static void sendCC(int potIndex, int value);
    static void sendModulatedCC(int arcIndex, int value, int targetPage, int targetChannel);
    static void updateGlobalValueSync(uint8_t cc, uint8_t channel, int value, int initiatorIndex = -1);
    static void updateModulation(float mixedValue);
    
    static void sendNoteOn(uint8_t note, uint8_t channel);
    static void sendNoteOff(uint8_t note, uint8_t channel);
    static void sendTrackActionPress(int buttonIndex);
    static void sendTrackActionRelease(int buttonIndex);
    static void sendTrackLayerPress(int layerIndex);
    static void sendTrackLayerRelease(int layerIndex);
    static void sendTemplateActionPress(int index);
    static void sendTemplateActionRelease(int index);
    static void sendTemplateArcActionPress(int arcIndex);
    static void sendTemplateArcActionRelease(int arcIndex);

    // Helpers
    static Channel getMIDIChannel(int channelIndex);
    static void loadValuesForCurrentState();
    static String getGlobalLabel(uint8_t cc, uint8_t channel);
    static String getMixerLabel(int page, int arcIndex);
    // Modulation state
    static int selectedArcForModulation;
    static int modulationTargetPage;
    static int modulationTargetChannel;
    static bool modulationEnabled;

    static bool ignoreIncomingMIDI;
    static unsigned long lastExternalUpdate[20];
    static unsigned long lastLocalUpdate[20];
    static bool needsResync;

private:
    struct Callbacks : FineGrainedMIDI_Callbacks<Callbacks> {
        void onControlChange(Channel channel, uint8_t controller, uint8_t value, Cable cable) {
            Serial.printf("MIDI RX: Ch %d, CC %d, Val %d\n", channel.getRaw() + 1, controller, value);
            MidiManager::handleIncomingMIDI(channel, controller, value);
        }
    };
    static Callbacks midi_callbacks;
};

#endif // MIDI_MANAGER_H
