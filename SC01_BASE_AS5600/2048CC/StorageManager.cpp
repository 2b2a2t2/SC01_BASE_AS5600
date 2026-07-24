#include "StorageManager.h"
#include "UiManager.h"
#include "MidiManager.h"
#include "LfoEngine.h"
#include <ArduinoJson.h>

unsigned long StorageManager::lastSaveTime = 0;
void* StorageManager::labelsCacheDoc = nullptr;

bool StorageManager::init() {
    Serial.println("Initializing SD card...");
    SPI.begin(PIN_SD_SCK, PIN_SD_MISO, PIN_SD_MOSI, PIN_SD_CS);
    if (!SD.begin(PIN_SD_CS, SPI, 4000000)) {
        Serial.println("ERROR: SD Card initialization failed!");
        return false;
    }
    Serial.println("SD Card initialized successfully.");

    // Populate memory cache for labels to prevent SD block latency
    labelsCacheDoc = new JsonDocument();
    if (SD.exists("/arc_labels.json")) {
        File file = SD.open("/arc_labels.json", FILE_READ);
        if (file) {
            deserializeJson(*(JsonDocument*)labelsCacheDoc, file);
            file.close();
        }
    }
    return true;
}

void StorageManager::listFiles() {
    File root = SD.open("/");
    if (!root) return;
    File file = root.openNextFile();
    while (file) {
        Serial.print(file.isDirectory() ? "DIR: " : "FILE: ");
        Serial.print(file.name());
        if (!file.isDirectory()) {
            Serial.print(" ("); Serial.print(file.size()); Serial.print(" bytes)");
        }
        Serial.println();
        file = root.openNextFile();
    }
    root.close();
}

bool StorageManager::saveConfig() {
    JsonDocument doc;
    doc["magic"] = "MIDI";
    doc["version"] = 12;

    doc["currentPage"] = MidiManager::currentPage;
    doc["currentMidiChannel"] = MidiManager::currentMidiChannel;

    JsonArray chPages = doc.createNestedArray("channelPages");
    JsonArray chRows = doc.createNestedArray("channelRows");
    for (int i = 0; i < NUM_CHANNELS; i++) {
        chPages.add(MidiManager::channelPages[i]);
        chRows.add(MidiManager::channelRows[i]);
    }

    doc["channelButtonTargetChannel"] = MidiManager::channelButtonTargetChannel;
    doc["mixerButtonNote"] = MidiManager::mixerButtonNote;

    JsonArray chNotes = doc.createNestedArray("channelButtonNotes");
    for (int i = 0; i < 16; i++) {
        chNotes.add(MidiManager::channelButtonNotes[i]);
    }

    // LFO State
    JsonObject lfo = doc.createNestedObject("lfo");
    JsonArray mixAmts = lfo.createNestedArray("mixAmounts");
    for (int i = 0; i < 4; i++) {
        mixAmts.add(LfoEngine::mixAmounts[i]);
    }
    lfo["selectedArcForModulation"] = MidiManager::selectedArcForModulation;
    lfo["modulationTargetPage"] = MidiManager::modulationTargetPage;
    lfo["modulationTargetChannel"] = MidiManager::modulationTargetChannel;

    JsonArray lfoUnits = lfo.createNestedArray("lfos");
    for (int i = 0; i < 4; i++) {
        JsonObject unit = lfoUnits.createNestedObject();
        unit["depth"] = LfoEngine::lfos[i].depth;
        unit["rate"] = LfoEngine::lfos[i].rate;
        unit["shape"] = LfoEngine::lfos[i].shape;
        unit["offset"] = LfoEngine::lfos[i].offset;
    }

    // Mixer state
    JsonObject mixer = doc.createNestedObject("mixer");
    JsonArray mixerBtnLabels = mixer.createNestedArray("buttonLabels");
    for (int i = 0; i < 5; i++) {
        mixerBtnLabels.add(MidiManager::mixerButtonLabels[i]);
    }

    JsonArray mixerPages = mixer.createNestedArray("pages");
    for (int p = 0; p < 5; p++) {
        JsonObject pageObj = mixerPages.createNestedObject();
        JsonArray vals = pageObj.createNestedArray("values");
        JsonArray ccs = pageObj.createNestedArray("ccs");
        JsonArray chs = pageObj.createNestedArray("channels");
        for (int i = 0; i < 16; i++) {
            vals.add(MidiManager::mixerPageValues[p][i]);
            ccs.add(MidiManager::mixerPageCCs[p][i]);
            chs.add(MidiManager::mixerPageChannels[p][i]);
        }
    }

    // Track Actions state
    JsonObject track = doc.createNestedObject("trackActions");
    JsonArray layerLabels = track.createNestedArray("layerLabels");
    for (int i = 0; i < 4; i++) {
        layerLabels.add(MidiManager::trackLayerLabels[i]);
    }

    JsonArray layerActions = track.createNestedArray("layerActions");
    for (int i = 0; i < 4; i++) {
        JsonObject la = layerActions.createNestedObject();
        la["type"] = MidiManager::trackLayerActions[i].type;
        la["value"] = MidiManager::trackLayerActions[i].value;
        la["channel"] = MidiManager::trackLayerActions[i].channel;
    }

    JsonArray actions = track.createNestedArray("actions");
    for (int l = 0; l < 4; l++) {
        JsonArray layerActionsArr = actions.createNestedArray();
        for (int b = 0; b < 4; b++) {
            JsonObject action = layerActionsArr.createNestedObject();
            action["type"] = MidiManager::trackActions[l][b].type;
            action["value"] = MidiManager::trackActions[l][b].value;
            action["channel"] = MidiManager::trackActions[l][b].channel;
            action["label"] = MidiManager::trackActions[l][b].label;
        }
    }

    // Template state
    JsonObject templateObj = doc.createNestedObject("templateActions");
    JsonArray tLabels = templateObj.createNestedArray("labels");
    for (int i = 0; i < 8; i++) tLabels.add(MidiManager::templateLabels[i]);
    JsonArray tNotes = templateObj.createNestedArray("notes");
    for (int i = 0; i < 8; i++) tNotes.add(MidiManager::templateNotes[i]);
    templateObj["pin12Note"] = MidiManager::templatePin12Note;
    templateObj["pin14Note"] = MidiManager::templatePin14Note;

    // Potentiometers CC values flat array
    JsonArray pots = doc.createNestedArray("potentiometers");
    for (int page = 0; page < NUM_PAGES; page++) {
        for (int ch = 0; ch < NUM_CHANNELS; ch++) {
            for (int pot = 0; pot < NUM_POTS; pot++) {
                pots.add(MidiManager::storedMidiCCValues[page][ch][pot]);
            }
        }
    }

    // Potentiometers Detents flat array
    JsonArray detents = doc.createNestedArray("detents");
    for (int page = 0; page < NUM_PAGES; page++) {
        for (int ch = 0; ch < NUM_CHANNELS; ch++) {
            for (int pot = 0; pot < NUM_POTS; pot++) {
                detents.add(MidiManager::storedPotentiometerDetents[page][ch][pot]);
            }
        }
    }

    // Potentiometers Modes flat array
    JsonArray modes = doc.createNestedArray("modes");
    for (int page = 0; page < NUM_PAGES; page++) {
        for (int ch = 0; ch < NUM_CHANNELS; ch++) {
            for (int pot = 0; pot < NUM_POTS; pot++) {
                modes.add(MidiManager::storedPotentiometerModes[page][ch][pot]);
            }
        }
    }

    File file = SD.open("/midi_config.json", FILE_WRITE);
    if (!file) {
        Serial.println("ERROR: Could not open midi_config.json for writing");
        return false;
    }

    serializeJson(doc, file);
    file.close();
    Serial.println("StorageManager: Saved config to JSON successfully.");
    lastSaveTime = millis();
    return true;
}

bool StorageManager::loadConfig() {
    // If a previous migration was incomplete (e.g. ran out of memory), restore backup binary files first!
    if (SD.exists("/midi_config.bin.bak") && !SD.exists("/midi_config.bin")) {
        bool needsRestore = false;
        if (!SD.exists("/midi_config.json")) {
            needsRestore = true;
        } else {
            File f = SD.open("/midi_config.json", FILE_READ);
            if (f) {
                JsonDocument checkDoc;
                DeserializationError err = deserializeJson(checkDoc, f);
                f.close();
                if (err || !checkDoc.containsKey("potentiometers") || checkDoc["potentiometers"].size() != NUM_PAGES * NUM_CHANNELS * NUM_POTS) {
                    needsRestore = true;
                }
            } else {
                needsRestore = true;
            }
        }

        if (needsRestore) {
            Serial.println("StorageManager: Detected incomplete previous migration. Restoring backup binaries for clean re-migration...");
            SD.remove("/midi_config.json");
            SD.remove("/arc_labels.json");
            SD.rename("/midi_config.bin.bak", "/midi_config.bin");
            if (SD.exists("/arc_labels.bin.bak")) {
                SD.rename("/arc_labels.bin.bak", "/arc_labels.bin");
            }
        }
    }

    // Perform automatic migration first if old binary exists and JSON doesn't
    if (SD.exists("/midi_config.bin") && !SD.exists("/midi_config.json")) {
        migrateBinaryToJSON();
    }

    if (!SD.exists("/midi_config.json")) {
        Serial.println("StorageManager: No config JSON found. Creating default...");
        return saveConfig();
    }

    File file = SD.open("/midi_config.json", FILE_READ);
    if (!file) {
        Serial.println("ERROR: Could not open midi_config.json for reading");
        return false;
    }

    JsonDocument doc;
    DeserializationError error = deserializeJson(doc, file);
    file.close();

    if (error) {
        Serial.print("ERROR: Failed to parse midi_config.json: ");
        Serial.println(error.c_str());
        return false;
    }

    MidiManager::currentPage = doc["currentPage"] | 0;
    MidiManager::currentMidiChannel = doc["currentMidiChannel"] | 4;

    JsonArray chPages = doc["channelPages"];
    JsonArray chRows = doc["channelRows"];
    for (int i = 0; i < NUM_CHANNELS; i++) {
        if (i < (int)chPages.size()) MidiManager::channelPages[i] = chPages[i];
        if (i < (int)chRows.size()) MidiManager::channelRows[i] = chRows[i];
    }
    UiManager::ccRowIndex = MidiManager::channelRows[MidiManager::currentMidiChannel];

    MidiManager::channelButtonTargetChannel = doc["channelButtonTargetChannel"] | 0;
    MidiManager::mixerButtonNote = doc["mixerButtonNote"] | 75;

    JsonArray chNotes = doc["channelButtonNotes"];
    for (int i = 0; i < 16 && i < (int)chNotes.size(); i++) {
        MidiManager::channelButtonNotes[i] = chNotes[i];
    }

    // LFO State
    if (doc.containsKey("lfo")) {
        JsonObject lfo = doc["lfo"];
        JsonArray mixAmts = lfo["mixAmounts"];
        for (int i = 0; i < 4 && i < (int)mixAmts.size(); i++) {
            LfoEngine::mixAmounts[i] = mixAmts[i];
        }
        MidiManager::selectedArcForModulation = lfo["selectedArcForModulation"] | 1;
        MidiManager::modulationTargetPage = lfo["modulationTargetPage"] | 0;
        MidiManager::modulationTargetChannel = lfo["modulationTargetChannel"] | 4;

        JsonArray lfoUnits = lfo["lfos"];
        for (int i = 0; i < 4 && i < (int)lfoUnits.size(); i++) {
            JsonObject unit = lfoUnits[i];
            LfoEngine::lfos[i].depth = unit["depth"] | 0.5f;
            LfoEngine::lfos[i].rate = unit["rate"] | 1.0f;
            LfoEngine::lfos[i].shape = unit["shape"] | 0;
            LfoEngine::lfos[i].offset = unit["offset"] | 0.5f;
        }
    }

    // Mixer state
    if (doc.containsKey("mixer")) {
        JsonObject mixer = doc["mixer"];
        JsonArray mixerBtnLabels = mixer["buttonLabels"];
        for (int i = 0; i < 5 && i < (int)mixerBtnLabels.size(); i++) {
            MidiManager::mixerButtonLabels[i] = mixerBtnLabels[i].as<String>();
        }

        JsonArray mixerPages = mixer["pages"];
        for (int p = 0; p < 5 && p < (int)mixerPages.size(); p++) {
            JsonObject pageObj = mixerPages[p];
            JsonArray vals = pageObj["values"];
            JsonArray ccs = pageObj["ccs"];
            JsonArray chs = pageObj["channels"];
            for (int i = 0; i < 16; i++) {
                if (i < (int)vals.size()) MidiManager::mixerPageValues[p][i] = vals[i];
                if (i < (int)ccs.size()) MidiManager::mixerPageCCs[p][i] = ccs[i];
                if (i < (int)chs.size()) MidiManager::mixerPageChannels[p][i] = chs[i];
            }
        }
    }

    // Track Actions state
    if (doc.containsKey("trackActions")) {
        JsonObject track = doc["trackActions"];
        JsonArray layerLabels = track["layerLabels"];
        for (int i = 0; i < 4 && i < (int)layerLabels.size(); i++) {
            MidiManager::trackLayerLabels[i] = layerLabels[i].as<String>();
        }

        if (track.containsKey("layerActions")) {
            JsonArray layerBtns = track["layerActions"];
            for (int i = 0; i < 4 && i < (int)layerBtns.size(); i++) {
                JsonObject la = layerBtns[i];
                MidiManager::trackLayerActions[i].type = la["type"] | 0;
                MidiManager::trackLayerActions[i].value = la["value"] | (24 + i);
                MidiManager::trackLayerActions[i].channel = la["channel"] | 0;
                MidiManager::trackLayerActions[i].label = MidiManager::trackLayerLabels[i];
            }
        } else {
            for (int i = 0; i < 4; i++) {
                MidiManager::trackLayerActions[i].type = 0;
                MidiManager::trackLayerActions[i].value = 24 + i;
                MidiManager::trackLayerActions[i].channel = 0;
                MidiManager::trackLayerActions[i].label = MidiManager::trackLayerLabels[i];
            }
        }

        JsonArray actions = track["actions"];
        for (int l = 0; l < 4 && l < (int)actions.size(); l++) {
            JsonArray subActions = actions[l];
            for (int b = 0; b < 4 && b < (int)subActions.size(); b++) {
                JsonObject action = subActions[b];
                MidiManager::trackActions[l][b].type = action["type"] | 1;
                MidiManager::trackActions[l][b].value = action["value"] | (36 + (l * 4) + b);
                MidiManager::trackActions[l][b].channel = action["channel"] | 0;
                MidiManager::trackActions[l][b].label = action["label"] | "";
            }
        }
    }

    // Template state
    if (doc.containsKey("templateActions")) {
        JsonObject templateObj = doc["templateActions"];
        if (templateObj.containsKey("labels")) {
            JsonArray labels = templateObj["labels"];
            for (int i = 0; i < 8 && i < (int)labels.size(); i++) MidiManager::templateLabels[i] = labels[i].as<String>();
        }
        if (templateObj.containsKey("notes")) {
            JsonArray notes = templateObj["notes"];
            for (int i = 0; i < 8 && i < (int)notes.size(); i++) MidiManager::templateNotes[i] = notes[i].as<uint8_t>();
        }
        if (templateObj.containsKey("pin12Note")) MidiManager::templatePin12Note = templateObj["pin12Note"].as<uint8_t>();
        if (templateObj.containsKey("pin14Note")) MidiManager::templatePin14Note = templateObj["pin14Note"].as<uint8_t>();
    }

    // Potentiometer values
    JsonArray pots = doc["potentiometers"];
    if (pots.size() == NUM_PAGES * NUM_CHANNELS * NUM_POTS) {
        int idx = 0;
        for (int page = 0; page < NUM_PAGES; page++) {
            for (int ch = 0; ch < NUM_CHANNELS; ch++) {
                for (int pot = 0; pot < NUM_POTS; pot++) {
                    MidiManager::storedMidiCCValues[page][ch][pot] = pots[idx];
                    MidiManager::storedPotentiometerValues[page][ch][pot] = (float)pots[idx];
                    idx++;
                }
            }
        }
    }

    // Potentiometer detents
    if (doc.containsKey("detents")) {
        JsonArray detents = doc["detents"];
        if (detents.size() == NUM_PAGES * NUM_CHANNELS * NUM_POTS) {
            int idx = 0;
            for (int page = 0; page < NUM_PAGES; page++) {
                for (int ch = 0; ch < NUM_CHANNELS; ch++) {
                    for (int pot = 0; pot < NUM_POTS; pot++) {
                        MidiManager::storedPotentiometerDetents[page][ch][pot] = detents[idx].as<bool>();
                        idx++;
                    }
                }
            }
        }
    }

    // Potentiometer modes
    if (doc.containsKey("modes")) {
        JsonArray modes = doc["modes"];
        if (modes.size() == NUM_PAGES * NUM_CHANNELS * NUM_POTS) {
            int idx = 0;
            for (int page = 0; page < NUM_PAGES; page++) {
                for (int ch = 0; ch < NUM_CHANNELS; ch++) {
                    for (int pot = 0; pot < NUM_POTS; pot++) {
                        MidiManager::storedPotentiometerModes[page][ch][pot] = modes[idx].as<uint8_t>();
                        idx++;
                    }
                }
            }
        }
    } else {
        for (int page = 0; page < NUM_PAGES; page++) {
            for (int ch = 0; ch < NUM_CHANNELS; ch++) {
                for (int pot = 0; pot < NUM_POTS; pot++) {
                    MidiManager::storedPotentiometerModes[page][ch][pot] = 0;
                }
            }
        }
    }

    MidiManager::currentCCBase = MidiManager::currentPage * 16;
    Serial.println("StorageManager: Loaded config from JSON successfully.");
    return true;
}

bool StorageManager::migrateBinaryToJSON() {
    if (!SD.exists("/midi_config.bin")) {
        return false;
    }

    Serial.println("StorageManager: Starting migration from binary to JSON...");

    File file = SD.open("/midi_config.bin", FILE_READ);
    if (!file) {
        Serial.println("ERROR: Could not open old midi_config.bin for migration");
        return false;
    }

    uint32_t magic;
    uint16_t version;
    file.read((uint8_t *)&magic, 4);
    file.read((uint8_t *)&version, 2);

    if (magic != 0x4D494449) {
        Serial.println("ERROR: Invalid magic number in binary config!");
        file.close();
        return false;
    }

    uint16_t numPages, numChannels, numPots;
    file.read((uint8_t *)&numPages, 2);
    file.read((uint8_t *)&numChannels, 2);
    file.read((uint8_t *)&numPots, 2);

    for (int page = 0; page < NUM_PAGES; page++) {
        for (int ch = 0; ch < NUM_CHANNELS; ch++) {
            for (int pot = 0; pot < NUM_POTS; pot++) {
                file.read((uint8_t *)&MidiManager::storedMidiCCValues[page][ch][pot], sizeof(int));
                file.read((uint8_t *)&MidiManager::storedPotentiometerValues[page][ch][pot], sizeof(float));
            }
        }
    }

    file.read((uint8_t *)&MidiManager::currentPage, sizeof(int));
    file.read((uint8_t *)&MidiManager::currentMidiChannel, sizeof(int));

    // Read per-channel page state if version is >= 2
    if (version >= 2) {
        file.read((uint8_t *)MidiManager::channelPages, sizeof(int) * NUM_CHANNELS);
    } else {
        for (int i = 0; i < NUM_CHANNELS; i++) {
            MidiManager::channelPages[i] = MidiManager::currentPage;
        }
    }

    // Read per-channel row state if version >= 11
    if (version >= 11) {
        file.read((uint8_t *)MidiManager::channelRows, sizeof(int) * NUM_CHANNELS);
        UiManager::ccRowIndex = MidiManager::channelRows[MidiManager::currentMidiChannel];
    } else {
        for (int i = 0; i < NUM_CHANNELS; i++) {
            MidiManager::channelRows[i] = 0;
        }
        UiManager::ccRowIndex = 0;
    }

    // Read LFO state if version >= 3
    if (version >= 3) {
        for (int i = 0; i < 4; i++) {
            file.read((uint8_t *)&LfoEngine::lfos[i].depth, sizeof(float));
            file.read((uint8_t *)&LfoEngine::lfos[i].rate, sizeof(float));
            file.read((uint8_t *)&LfoEngine::lfos[i].shape, sizeof(int));
            file.read((uint8_t *)&LfoEngine::lfos[i].offset, sizeof(float));
        }
        file.read((uint8_t *)LfoEngine::mixAmounts, sizeof(float) * 4);

        int modulationTarget;
        file.read((uint8_t *)&modulationTarget, sizeof(int));
        MidiManager::selectedArcForModulation = modulationTarget;

        if (version >= 4) {
            file.read((uint8_t *)&MidiManager::modulationTargetPage, sizeof(int));
            file.read((uint8_t *)&MidiManager::modulationTargetChannel, sizeof(int));
        }
    }

    // Read channel button notes if version >= 5
    if (version >= 5) {
        file.read((uint8_t *)MidiManager::channelButtonNotes, 16);
        file.read((uint8_t *)&MidiManager::channelButtonTargetChannel, 1);
        file.read((uint8_t *)&MidiManager::mixerButtonNote, 1);
    }

    // Read Mixer Pages (Version 8)
    if (version >= 8) {
        file.read((uint8_t *)MidiManager::mixerPageValues, sizeof(float) * 5 * 16);
        file.read((uint8_t *)MidiManager::mixerPageCCs, 5 * 16);
        file.read((uint8_t *)MidiManager::mixerPageChannels, 5 * 16);
        if (version < 10) {
            for (int p = 0; p < 5; p++) {
                for (int i = 0; i < 16; i++) {
                    uint8_t len;
                    if (file.read(&len, 1) != 1) break;
                    char buffer[256];
                    file.read((uint8_t *)buffer, len);
                }
            }
        }
        for (int i = 0; i < 5; i++) {
            uint8_t len;
            file.read(&len, 1);
            char buffer[256];
            file.read((uint8_t *)buffer, len);
            buffer[len] = '\0';
            MidiManager::mixerButtonLabels[i] = String(buffer);
        }
    }

    // Read Track Actions (Version 9)
    if (version >= 9) {
        for (int l = 0; l < 4; l++) {
            for (int b = 0; b < 4; b++) {
                uint8_t isNote;
                file.read((uint8_t *)&isNote, 1);
                MidiManager::trackActions[l][b].type = isNote ? 1 : 2;
                file.read((uint8_t *)&MidiManager::trackActions[l][b].value, 1);
                file.read((uint8_t *)&MidiManager::trackActions[l][b].channel, 1);
                uint8_t len;
                if (file.read(&len, 1) != 1) break;
                char buffer[256];
                file.read((uint8_t *)buffer, len);
                buffer[len] = '\0';
                MidiManager::trackActions[l][b].label = String(buffer);
            }
        }
    }

    MidiManager::currentCCBase = MidiManager::currentPage * 16;
    file.close();

    // Migrate labels from `/arc_labels.bin` if it exists
    if (SD.exists("/arc_labels.bin")) {
        Serial.println("StorageManager: Migrating labels from arc_labels.bin...");
        File labelsFile = SD.open("/arc_labels.bin", FILE_READ);
        if (labelsFile) {
            JsonDocument doc;
            JsonObject root = doc.to<JsonObject>();

            for (int page = 0; page < NUM_PAGES; page++) {
                for (int ch = 0; ch < NUM_CHANNELS; ch++) {
                    for (int pot = 0; pot < NUM_POTS; pot++) {
                        char buffer[LABEL_RECORD_SIZE];
                        labelsFile.read((uint8_t*)buffer, LABEL_RECORD_SIZE);
                        buffer[LABEL_RECORD_SIZE - 1] = '\0';
                        String label = String(buffer);
                        String defaultLabel = "CC " + String(page * 16 + pot + 1);
                        if (label.length() > 0 && label != defaultLabel) {
                            String key = String(page) + "_" + String(ch) + "_" + String(pot);
                            root[key] = label;
                        }
                    }
                }
            }
            labelsFile.close();

            // Save labels JSON
            File outLabels = SD.open("/arc_labels.json", FILE_WRITE);
            if (outLabels) {
                serializeJson(doc, outLabels);
                outLabels.close();
                Serial.println("StorageManager: Labels migration successful!");
                if (labelsCacheDoc) {
                    *(JsonDocument*)labelsCacheDoc = doc; // Update RAM cache
                }
            }
            
            // Backup old binary labels
            SD.rename("/arc_labels.bin", "/arc_labels.bin.bak");
        }
    }

    // Save configuration directly to new JSON format
    bool saved = saveConfig();
    if (saved) {
        Serial.println("StorageManager: Configuration migration successful!");
        SD.rename("/midi_config.bin", "/midi_config.bin.bak");
    }

    return saved;
}

bool StorageManager::saveLabels() {
    return true; 
}

bool StorageManager::loadLabels() {
    loadLabelsForCurrentState();
    return true;
}

void StorageManager::initLabels() {
    // Replaced by loadConfig migration logic
}

void StorageManager::saveLabel(int page, int channel, int pot, String label) {
    if (page < 0 || page >= NUM_PAGES || channel < 0 || channel >= NUM_CHANNELS || pot < 0 || pot >= NUM_POTS) return;

    if (!labelsCacheDoc) {
        labelsCacheDoc = new JsonDocument();
    }

    JsonDocument* cache = (JsonDocument*)labelsCacheDoc;
    String key = String(page) + "_" + String(channel) + "_" + String(pot);
    String defaultLabel = "CC " + String(page * 16 + pot + 1);

    if (label.length() > 0 && label != defaultLabel) {
        (*cache)[key] = label;
    } else {
        cache->remove(key);
    }

    if (page == MidiManager::currentPage && channel == MidiManager::currentMidiChannel) {
        MidiManager::currentArcLabels[pot] = label;
    }

    // Also update mixer display cache when editing from mixer mode
    if (UiManager::isMixerMode) {
        for (int i = 0; i < 16; i++) {
            uint8_t cc = MidiManager::mixerPageCCs[UiManager::currentMixerPage][i];
            uint8_t ch = MidiManager::mixerPageChannels[UiManager::currentMixerPage][i];
            if (cc >= 1 && cc <= 128) {
                int targetPage = (cc - 1) / 16;
                int targetPot = (cc - 1) % 16;
                if (targetPage == page && targetPot == pot && ch == channel) {
                    MidiManager::currentMixerArcLabels[i] = label;
                    break;
                }
            }
        }
    }

    File file = SD.open("/arc_labels.json", FILE_WRITE);
    if (file) {
        serializeJson(*cache, file);
        file.close();
    }

    lastSaveTime = millis();
}

String StorageManager::getLabel(int page, int channel, int pot) {
    if (page < 0 || page >= NUM_PAGES || channel < 0 || channel >= NUM_CHANNELS || pot < 0 || pot >= NUM_POTS) return "";

    if (page == MidiManager::currentPage && channel == MidiManager::currentMidiChannel) {
        if (MidiManager::currentArcLabels[pot].length() > 0) {
            return MidiManager::currentArcLabels[pot];
        }
    }

    String defaultLabel = "CC " + String(page * 16 + pot + 1);

    if (!labelsCacheDoc) return defaultLabel;

    JsonDocument* cache = (JsonDocument*)labelsCacheDoc;
    String key = String(page) + "_" + String(channel) + "_" + String(pot);
    if (cache->containsKey(key)) {
        return (*cache)[key].as<String>();
    }

    return defaultLabel;
}

void StorageManager::loadLabelsForCurrentState() {
    int page = MidiManager::currentPage;
    int channel = MidiManager::currentMidiChannel;

    for (int i = 0; i < NUM_POTS; i++) {
        MidiManager::currentArcLabels[i] = "CC " + String(page * 16 + i + 1);
    }

    if (!labelsCacheDoc) return;

    JsonDocument* cache = (JsonDocument*)labelsCacheDoc;
    for (int i = 0; i < NUM_POTS; i++) {
        String key = String(page) + "_" + String(channel) + "_" + String(i);
        if (cache->containsKey(key)) {
            MidiManager::currentArcLabels[i] = (*cache)[key].as<String>();
        }
    }
}

void StorageManager::loadMixerLabelsForCurrentState() {
    int mixerPage = UiManager::currentMixerPage;

    for (int i = 0; i < 16; i++) {
        uint8_t cc = MidiManager::mixerPageCCs[mixerPage][i];
        MidiManager::currentMixerArcLabels[i] = "CC " + String(cc);
    }

    if (!labelsCacheDoc) return;

    JsonDocument* cache = (JsonDocument*)labelsCacheDoc;
    for (int i = 0; i < 16; i++) {
        uint8_t cc = MidiManager::mixerPageCCs[mixerPage][i];
        uint8_t ch = MidiManager::mixerPageChannels[mixerPage][i];

        if (cc < 1 || cc > 128) continue;

        int targetPage = (cc - 1) / 16;
        int targetPot = (cc - 1) % 16;

        String key = String(targetPage) + "_" + String(ch) + "_" + String(targetPot);
        if (cache->containsKey(key)) {
            String label = (*cache)[key].as<String>();
            if (label.length() > 0 && !label.startsWith("CC ")) {
                MidiManager::currentMixerArcLabels[i] = label;
            }
        }
    }
}

void StorageManager::handleAutoSave() {
    unsigned long currentTime = millis();
    if (currentTime - lastSaveTime > AUTO_SAVE_INTERVAL) {
        if (saveConfig()) {
            Serial.println("Auto-save: Configuration saved to JSON on SD.");
        } else {
            Serial.println("Auto-save ERROR: Failed to save config to JSON.");
        }
        lastSaveTime = currentTime;
    }
}
