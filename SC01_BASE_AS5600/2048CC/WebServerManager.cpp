#include "WebServerManager.h"
#include <ArduinoJson.h>
#include "esp_bt.h"
#include "HardwareHAL.h"
#include "src/UI/ui.h"

// Static member definitions
WebServer WebServerManager::webServer(80);
File WebServerManager::uploadFile;
bool WebServerManager::wifiEnabled = true;
bool WebServerManager::wifiConnected = false;
unsigned long WebServerManager::wifiConnectStartTime = 0;

// Externs for SSID/Password from Config.cpp
extern const char* ssid;
extern const char* password;

void WebServerManager::init() {
    webServer.on("/", handleWebRoot);
    webServer.on("/api/config", HTTP_GET, handleWebAPI);
    webServer.on("/api/config", HTTP_POST, handleWebAPI);
    webServer.on("/api/buttons", HTTP_GET, handleButtonsAPI);
    webServer.on("/api/buttons", HTTP_POST, handleButtonsAPI);
    webServer.on("/api/mixer-config", HTTP_GET, handleMixerConfigAPI);
    webServer.on("/api/mixer-config", HTTP_POST, handleMixerConfigAPI);
    webServer.on("/api/track-actions", HTTP_GET, handleTrackActionsAPI);
    webServer.on("/api/track-actions", HTTP_POST, handleTrackActionsAPI);
    webServer.on("/api/template-actions", HTTP_GET, handleTemplateActionsAPI);
    webServer.on("/api/template-actions", HTTP_POST, handleTemplateActionsAPI);
    webServer.on("/api/files", HTTP_GET, handleFileList);
    webServer.on("/api/files", HTTP_DELETE, handleFileDelete);
    webServer.on("/api/download", HTTP_GET, handleFileDownload);
    webServer.on("/api/upload", HTTP_POST, [](){ webServer.send(200); }, handleFileUpload);
    webServer.onNotFound([]() { 
        String uri = webServer.uri();
        if (SD.exists(uri)) {
            File file = SD.open(uri, FILE_READ);
            String contentType = "application/octet-stream";
            if (uri.endsWith(".jpeg") || uri.endsWith(".jpg")) contentType = "image/jpeg";
            else if (uri.endsWith(".gif")) contentType = "image/gif";
            else if (uri.endsWith(".png")) contentType = "image/png";
            else if (uri.endsWith(".css")) contentType = "text/css";
            else if (uri.endsWith(".js")) contentType = "application/javascript";
            webServer.streamFile(file, contentType);
            file.close();
            return;
        }
        webServer.send(404, "text/plain", "Not found"); 
    });
}

void WebServerManager::startWiFi() {
    if (!wifiEnabled) {
        wifiEnabled = true;
        wifiConnected = false;
        Serial.println("\n=== STARTING WIFI ===");
        
        btStop();
        delay(500);
        
        WiFi.disconnect(true);
        WiFi.mode(WIFI_OFF);
        delay(100);
        
        WiFi.mode(WIFI_STA);
        WiFi.setHostname("wipidim");
        WiFi.begin(ssid, password);
        wifiConnectStartTime = millis();

        if (ui_ButtonWifi && ui_LabelWifi) {
            lv_label_set_text(ui_LabelWifi, "Connecting...");
            lv_obj_set_style_bg_color(ui_ButtonWifi, lv_color_hex(0xFF6600), LV_PART_MAIN | LV_STATE_DEFAULT);
        }
    }
}

void WebServerManager::stopWiFi() {
    if (wifiEnabled) {
        Serial.println("\n=== STOPPING WIFI ===");
        webServer.stop();
        WiFi.disconnect(true);
        WiFi.mode(WIFI_OFF);
        delay(100);
        
        wifiEnabled = false;
        wifiConnected = false;
        
        if (ui_ButtonWifi && ui_LabelWifi) {
            lv_label_set_text(ui_LabelWifi, "Connect WiFi");
            lv_obj_set_style_bg_color(ui_ButtonWifi, lv_color_hex(0x94AA00), LV_PART_MAIN | LV_STATE_DEFAULT);
        }
    }
}

void WebServerManager::handleWiFi() {
    if (!wifiEnabled) return;

    if (!wifiConnected) {
        wl_status_t status = WiFi.status();
        if (status == WL_CONNECTED) {
            wifiConnected = true;
            webServer.begin();
            Serial.println("\n✓✓✓ WIFI CONNECTED ✓✓✓");
            Serial.print("IP Address: ");
            Serial.println(WiFi.localIP());
            
            if (ui_ButtonWifi && ui_LabelWifi) {
                String ipStr = WiFi.localIP().toString();
                lv_label_set_text(ui_LabelWifi, ipStr.c_str());
                lv_obj_set_style_bg_color(ui_ButtonWifi, lv_color_hex(0x00AA00), LV_PART_MAIN | LV_STATE_DEFAULT);
            }
        } else if (millis() - wifiConnectStartTime > wifiConnectTimeout) {
            wifiEnabled = false;
            if (ui_ButtonWifi && ui_LabelWifi) {
                lv_label_set_text(ui_LabelWifi, "Failed");
                lv_obj_set_style_bg_color(ui_ButtonWifi, lv_color_hex(0xFF0000), LV_PART_MAIN | LV_STATE_DEFAULT);
            }
        }
    } else {
        webServer.handleClient();
    }
}

void WebServerManager::handleFileList() {
    JsonDocument doc;
    JsonArray arr = doc.to<JsonArray>();
    
    File root = SD.open("/");
    File file = root.openNextFile();
    while (file) {
        if (!file.isDirectory()) {
            JsonObject obj = arr.createNestedObject();
            obj["name"] = String(file.name());
            obj["size"] = file.size();
        }
        file = root.openNextFile();
    }
    
    String json;
    serializeJson(doc, json);
    webServer.send(200, "application/json", json);
}

void WebServerManager::handleFileDownload() {
    if (!webServer.hasArg("name")) {
        webServer.send(400, "text/plain", "Missing filename");
        return;
    }
    String filename = webServer.arg("name");
    if (!filename.startsWith("/")) filename = "/" + filename;
    
    if (SD.exists(filename)) {
        File file = SD.open(filename, FILE_READ);
        webServer.streamFile(file, "application/octet-stream");
        file.close();
    } else {
        webServer.send(404, "text/plain", "File not found");
    }
}

void WebServerManager::handleFileDelete() {
    if (!webServer.hasArg("name")) {
        webServer.send(400, "text/plain", "Missing filename");
        return;
    }
    String filename = webServer.arg("name");
    if (!filename.startsWith("/")) filename = "/" + filename;
    
    if (SD.exists(filename)) {
        if (SD.remove(filename)) {
            webServer.send(200, "application/json", "{\"message\":\"File deleted\"}");
        } else {
            webServer.send(500, "application/json", "{\"error\":\"Could not delete file\"}");
        }
    } else {
        webServer.send(404, "application/json", "{\"error\":\"File not found\"}");
    }
}

void WebServerManager::handleFileUpload() {
    HTTPUpload& upload = webServer.upload();
    if (upload.status == UPLOAD_FILE_START) {
        String filename = upload.filename;
        if (!filename.startsWith("/")) filename = "/" + filename;
        Serial.printf("Upload Start: %s\n", filename.c_str());
        uploadFile = SD.open(filename, FILE_WRITE);
    } else if (upload.status == UPLOAD_FILE_WRITE) {
        if (uploadFile) uploadFile.write(upload.buf, upload.currentSize);
    } else if (upload.status == UPLOAD_FILE_END) {
        if (uploadFile) uploadFile.close();
        Serial.printf("Upload End: %u bytes\n", upload.totalSize);
    }
}

void WebServerManager::handleWebRoot() {
    webServer.send(200, "text/html; charset=utf-8", getIndexHtml());
}

void WebServerManager::handleWebAPI() {
    if (webServer.method() == HTTP_GET) {
        int page = webServer.arg("page").toInt();
        int channel = webServer.arg("channel").toInt();
        if (page >= 0 && page < NUM_PAGES && channel >= 0 && channel < NUM_CHANNELS) {
            JsonDocument doc;
            JsonArray root = doc.to<JsonArray>();
            for (int pot = 0; pot < 16; pot++) {
                JsonObject p = root.createNestedObject();
                p["value"] = MidiManager::storedMidiCCValues[page][channel][pot];
                p["label"] = StorageManager::getLabel(page, channel, pot);
                p["detent"] = MidiManager::storedPotentiometerDetents[page][channel][pot];
                p["mode"] = MidiManager::storedPotentiometerModes[page][channel][pot];
            }
            String json;
            serializeJson(doc, json);
            webServer.send(200, "application/json; charset=utf-8", json);
        } else {
            webServer.send(400, "application/json; charset=utf-8", "{\"error\":\"Invalid page or channel\"}");
        }
    } else if (webServer.method() == HTTP_POST) {
        if (webServer.hasArg("plain")) {
            StaticJsonDocument<8192> doc;
            DeserializationError error = deserializeJson(doc, webServer.arg("plain"));
            if (!error) {
                int page = doc["page"];
                int channel = doc["channel"];
                JsonArray pots = doc["pots"];
                
                if (page >= 0 && page < NUM_PAGES && channel >= 0 && channel < NUM_CHANNELS) {
                    for (int i = 0; i < (int)pots.size() && i < 16; i++) {
                        MidiManager::storedMidiCCValues[page][channel][i] = pots[i]["value"];
                        MidiManager::storedPotentiometerValues[page][channel][i] = (float)pots[i]["value"];
                        MidiManager::storedPotentiometerDetents[page][channel][i] = pots[i]["detent"].as<bool>();
                        MidiManager::storedPotentiometerModes[page][channel][i] = pots[i]["mode"].as<uint8_t>();
                        StorageManager::saveLabel(page, channel, i, pots[i]["label"].as<String>());
                    }
                    StorageManager::saveConfig();
                    
                    if (page == MidiManager::currentPage && channel == MidiManager::currentMidiChannel) {
                        UiManager::loadValuesForCurrentState();
                    }
                    webServer.send(200, "application/json; charset=utf-8", "{\"message\":\"Configuration updated successfully\"}");
                    return;
                }
            }
            webServer.send(400, "application/json; charset=utf-8", "{\"error\":\"Invalid JSON or parameters\"}");
        }
    }
}

void WebServerManager::handleButtonsAPI() {
    if (webServer.method() == HTTP_GET) {
        StaticJsonDocument<1024> doc;
        doc["channel"] = MidiManager::channelButtonTargetChannel;
        doc["mixerNote"] = MidiManager::mixerButtonNote;
        JsonArray notes = doc.createNestedArray("notes");
        for (int i = 0; i < 16; i++) {
            notes.add(MidiManager::channelButtonNotes[i]);
        }
        String json;
        serializeJson(doc, json);
        webServer.send(200, "application/json; charset=utf-8", json);
    } else if (webServer.method() == HTTP_POST) {
        if (webServer.hasArg("plain")) {
            StaticJsonDocument<1024> doc;
            DeserializationError error = deserializeJson(doc, webServer.arg("plain"));
            if (!error) {
                MidiManager::channelButtonTargetChannel = doc["channel"];
                MidiManager::mixerButtonNote = doc["mixerNote"];
                JsonArray notes = doc["notes"];
                for (int i = 0; i < 16 && i < (int)notes.size(); i++) {
                    MidiManager::channelButtonNotes[i] = notes[i];
                }
                StorageManager::saveConfig();
                webServer.send(200, "application/json; charset=utf-8", "{\"message\":\"Button settings updated\"}");
                return;
            }
        }
        webServer.send(400, "application/json; charset=utf-8", "{\"error\":\"Invalid JSON\"}");
    }
}

void WebServerManager::handleMixerConfigAPI() {
    if (webServer.method() == HTTP_GET) {
        int page = webServer.hasArg("page") ? webServer.arg("page").toInt() : 0;
        JsonDocument doc;
        doc["buttonLabel"] = MidiManager::mixerButtonLabels[page];
        JsonArray arcs = doc.createNestedArray("arcs");
        for (int i = 0; i < 16; i++) {
            JsonObject a = arcs.createNestedObject();
            a["cc"] = MidiManager::mixerPageCCs[page][i];
            a["channel"] = MidiManager::mixerPageChannels[page][i];
            a["label"] = MidiManager::getGlobalLabel(a["cc"], a["channel"]);
        }
        
        // Include Page 1 labels for all channels (16x16)
        JsonArray p1 = doc.createNestedArray("page1Labels");
        for (int ch = 0; ch < 16; ch++) {
            JsonArray chLabels = p1.createNestedArray();
            for (int i = 0; i < 16; i++) {
                chLabels.add(StorageManager::getLabel(0, ch, i));
            }
        }

        String json;
        serializeJson(doc, json);
        webServer.send(200, "application/json; charset=utf-8", json);
    } else if (webServer.method() == HTTP_POST) {
        if (webServer.hasArg("plain")) {
            StaticJsonDocument<8192> doc;
            DeserializationError error = deserializeJson(doc, webServer.arg("plain"));
            if (!error) {
                int page = doc["page"];
                MidiManager::mixerButtonLabels[page] = doc["buttonLabel"].as<String>();
                JsonArray arcs = doc["arcs"];
                for (int i = 0; i < 16 && i < (int)arcs.size(); i++) {
                    uint8_t cc = arcs[i]["cc"];
                    uint8_t channel = arcs[i]["channel"];
                    String label = arcs[i]["label"].as<String>();
                    
                    MidiManager::mixerPageCCs[page][i] = cc;
                    MidiManager::mixerPageChannels[page][i] = channel;

                    // Sync back to standard labels if this matches a CC
                    if (cc >= 1 && cc <= 128) {
                        int targetPage = (cc - 1) / 16;
                        int targetPot = (cc - 1) % 16;
                        StorageManager::saveLabel(targetPage, channel, targetPot, label);
                    }
                }
                StorageManager::saveConfig();
                // StorageManager::saveLabels(); // No longer needed
                webServer.send(200, "application/json; charset=utf-8", "{\"message\":\"Mixer configuration updated\"}");
                return;
            }
        }
        webServer.send(400, "application/json; charset=utf-8", "{\"error\":\"Invalid JSON\"}");
    }
}

void WebServerManager::handleTrackActionsAPI() {
    if (webServer.method() == HTTP_GET) {
        JsonDocument doc;
        JsonObject root = doc.to<JsonObject>();

        JsonArray layerLabelsArr = root.createNestedArray("layerLabels");
        for (int i = 0; i < 4; i++) {
            layerLabelsArr.add(MidiManager::trackLayerLabels[i]);
        }

        JsonArray layerActionsArr = root.createNestedArray("layerActions");
        for (int i = 0; i < 4; i++) {
            JsonObject la = layerActionsArr.createNestedObject();
            la["type"] = MidiManager::trackLayerActions[i].type;
            la["value"] = MidiManager::trackLayerActions[i].value;
            la["channel"] = MidiManager::trackLayerActions[i].channel;
        }

        JsonArray layers = root.createNestedArray("actions");
        for (int l = 0; l < 4; l++) {
            JsonArray actions = layers.createNestedArray();
            for (int b = 0; b < 4; b++) {
                JsonObject a = actions.createNestedObject();
                a["type"] = MidiManager::trackActions[l][b].type;
                a["value"] = MidiManager::trackActions[l][b].value;
                a["channel"] = MidiManager::trackActions[l][b].channel;
                a["label"] = MidiManager::trackActions[l][b].label;
            }
        }
        String json;
        serializeJson(doc, json);
        webServer.send(200, "application/json; charset=utf-8", json);
    } else if (webServer.method() == HTTP_POST) {
        if (webServer.hasArg("plain")) {
            JsonDocument doc;
            DeserializationError error = deserializeJson(doc, webServer.arg("plain"));
            if (!error) {
                JsonObject root = doc.as<JsonObject>();

                if (root.containsKey("layerLabels")) {
                    JsonArray layerLabelsArr = root["layerLabels"];
                    for (int i = 0; i < 4 && i < (int)layerLabelsArr.size(); i++) {
                        MidiManager::trackLayerLabels[i] = layerLabelsArr[i].as<String>();
                    }
                }

                if (root.containsKey("layerActions")) {
                    JsonArray layerBtns = root["layerActions"];
                    for (int i = 0; i < 4 && i < (int)layerBtns.size(); i++) {
                        MidiManager::trackLayerActions[i].type = layerBtns[i]["type"];
                        MidiManager::trackLayerActions[i].value = layerBtns[i]["value"];
                        MidiManager::trackLayerActions[i].channel = layerBtns[i]["channel"];
                        MidiManager::trackLayerActions[i].label = MidiManager::trackLayerLabels[i];
                    }
                }

                if (root.containsKey("actions")) {
                    JsonArray layers = root["actions"];
                    for (int l = 0; l < 4 && l < (int)layers.size(); l++) {
                        JsonArray actions = layers[l];
                        for (int b = 0; b < 4 && b < (int)actions.size(); b++) {
                            MidiManager::trackActions[l][b].type = actions[b]["type"];
                            MidiManager::trackActions[l][b].value = actions[b]["value"];
                            MidiManager::trackActions[l][b].channel = actions[b]["channel"];
                            MidiManager::trackActions[l][b].label = actions[b]["label"].as<String>();
                        }
                    }
                }

                StorageManager::saveConfig();
                UiManager::updateTrackButtonLabels();
                webServer.send(200, "application/json; charset=utf-8", "{\"message\":\"Track actions updated\"}");
                return;
            }
        }
        webServer.send(400, "application/json; charset=utf-8", "{\"error\":\"Invalid JSON\"}");
    }
}

void WebServerManager::handleTemplateActionsAPI() {
    if (webServer.method() == HTTP_GET) {
        JsonDocument doc;
        JsonObject root = doc.to<JsonObject>();
        
        JsonArray labelsArr = root.createNestedArray("labels");
        for (int i = 0; i < 8; i++) labelsArr.add(MidiManager::templateLabels[i]);
        
        JsonArray notesArr = root.createNestedArray("notes");
        for (int i = 0; i < 8; i++) notesArr.add(MidiManager::templateNotes[i]);
        
        root["pin12Note"] = MidiManager::templatePin12Note;
        root["pin14Note"] = MidiManager::templatePin14Note;
        
        String json;
        serializeJson(doc, json);
        webServer.send(200, "application/json; charset=utf-8", json);
    } else if (webServer.method() == HTTP_POST) {
        if (webServer.hasArg("plain")) {
            JsonDocument doc;
            DeserializationError error = deserializeJson(doc, webServer.arg("plain"));
            if (!error) {
                JsonObject root = doc.as<JsonObject>();
                if (root.containsKey("labels")) {
                    JsonArray labelsArr = root["labels"];
                    for (int i = 0; i < 8 && i < (int)labelsArr.size(); i++) MidiManager::templateLabels[i] = labelsArr[i].as<String>();
                }
                if (root.containsKey("notes")) {
                    JsonArray notesArr = root["notes"];
                    for (int i = 0; i < 8 && i < (int)notesArr.size(); i++) MidiManager::templateNotes[i] = notesArr[i].as<uint8_t>();
                }
                if (root.containsKey("pin12Note")) MidiManager::templatePin12Note = root["pin12Note"].as<uint8_t>();
                if (root.containsKey("pin14Note")) MidiManager::templatePin14Note = root["pin14Note"].as<uint8_t>();
                
                StorageManager::saveConfig();
                webServer.send(200, "application/json; charset=utf-8", "{\"message\":\"Template actions updated\"}");
                return;
            }
        }
        webServer.send(400, "application/json; charset=utf-8", "{\"error\":\"Invalid JSON\"}");
    }
}

String WebServerManager::getIndexHtml() {
    String html = "<!DOCTYPE html><html><head><meta charset='UTF-8'><meta name='viewport' content='width=device-width, initial-scale=1'>";
    html += "<link rel='icon' href='/Logo.jpeg' type='image/jpeg'>";
    html += "<link rel='apple-touch-icon' href='/Logo.jpeg'>";
    html += "<title>2048CC MIDI Configuration</title><style>";
    html += "body{font-family:Arial;background:#1a1a1a;color:#fff;margin:0;padding:20px}.container{max-width:1200px;margin:0 auto}";
    html += ".header{text-align:center;margin-bottom:30px}.nav{display:flex;justify-content:center;gap:10px;margin-bottom:20px;flex-wrap:wrap}";
    html += ".nav button{padding:10px 20px;background:#333;color:white;border:none;border-radius:5px;cursor:pointer}.nav button.active{background:#4CAF50}";
    html += ".tab-nav{display:flex;justify-content:center;gap:20px;margin-bottom:30px;border-bottom:1px solid #444;padding-bottom:10px}";
    html += ".tab-nav button{background:none;border:none;color:#888;font-size:18px;cursor:pointer;padding:5px 15px}.tab-nav button.active{color:#4CAF50;border-bottom:2px solid #4CAF50}";
    html += ".channel-bar{display:flex;flex-wrap:wrap;gap:5px;margin-bottom:20px}.channel-btn{padding:8px 12px;background:#333;border:none;border-radius:3px;color:white;cursor:pointer}";
    html += ".channel-btn.active{background:#2196F3}.grid{display:grid;grid-template-columns:repeat(4,1fr);gap:10px}.card{background:#2a2a2a;border-radius:6px;padding:8px;text-align:center}";
    html += ".card input, .card textarea{width:100%;padding:4px;margin:4px 0;background:#333;border:1px solid #555;color:white;border-radius:4px;box-sizing:border-box}";
    html += ".card h3{margin:0 0 4px 0;font-size:14px;color:#aaa}.card textarea{height:40px;resize:vertical;font-family:inherit;font-size:12px}.card .value{font-size:18px;font-weight:bold;color:#4CAF50;margin:4px 0}";
    html += ".save-btn{background:#4CAF50;color:white;border:none;padding:12px 24px;border-radius:5px;cursor:pointer;font-size:16px;margin-top:20px}";
    html += ".status{margin-top:20px;padding:10px;background:#333;border-radius:5px;text-align:center}.hidden{display:none}</style>";
    html += "<script>let currentPage=0,currentChannel=0;";
    html += "function showTab(t){document.querySelectorAll('.tab-content').forEach(c=>c.classList.add('hidden'));document.querySelectorAll('.tab-btn').forEach(b=>b.classList.remove('active'));";
    html += "document.getElementById(t+'-tab').classList.remove('hidden');event.target.classList.add('active');if(t==='mixer')loadMixerConfig();if(t==='track')loadTrackActions();if(t==='template')loadTemplateActions();if(t==='files')loadFiles();}";
    html += "function loadPage(page,channel){currentPage=page;currentChannel=channel;";
    html += "fetch('/api/config?page='+page+'&channel='+channel).then(r=>r.json()).then(data=>{updateGrid(data);updateActiveStates(page,channel);});}";
    html += "function updateGrid(data){const grid=document.getElementById('grid');grid.innerHTML='';data.forEach((item,index)=>{const card=document.createElement('div');card.className='card';";
    html += "card.innerHTML='<div style=\"display:flex;justify-content:center;align-items:center;gap:8px;margin-bottom:4px\"><h3>Pot '+(index+1)+' :</h3><div class=\"value\" id=\"value-'+index+'\" style=\"margin:0\">'+item.value+'</div></div>';";
    html += "card.innerHTML+='<input type=\"range\" min=\"0\" max=\"127\" value=\"'+item.value+'\" oninput=\"updateValue('+index+',this.value)\" style=\"width:50%;margin:0 auto\">';";
    html += "card.innerHTML+='<textarea id=\"label-'+index+'\" placeholder=\"Label\">'+escapeHtml(item.label)+'</textarea>';";
    html += "card.innerHTML+='<div style=\"margin-top:4px;display:flex;flex-direction:column;align-items:center;gap:4px\"><label style=\"cursor:pointer;display:flex;align-items:center;gap:4px;font-size:14px\"><input type=\"checkbox\" id=\"detent-check-'+index+'\" '+(item.detent?'checked':'')+'>Detent</label><select id=\"mode-'+index+'\" style=\"width:100%;padding:4px;background:#333;color:white;border:1px solid #555;border-radius:4px;cursor:pointer\"><option value=\"0\" '+(item.mode===0?\"selected\":\"\")+\'>Absolute</option><option value=\"1\" '+(item.mode===1?\"selected\":\"\")+\'>Relative (7Fh/01h)</option><option value=\"2\" '+(item.mode===2?\"selected\":\"\")+\'>Relative (3Fh/41h)</option></select></div>\';";
    html += "grid.appendChild(card);});}";
    html += "function loadButtons(){fetch('/api/buttons').then(r=>r.json()).then(data=>{document.getElementById('btn-midi-ch').value=data.channel;document.getElementById('mixer-btn-note').value=data.mixerNote;const grid=document.getElementById('btn-grid');grid.innerHTML='';";
    html += "data.notes.forEach((note,i)=>{const card=document.createElement('div');card.className='card';card.innerHTML='<h3>Button '+(i+1)+'</h3><div class=\"value\" id=\"btn-val-'+i+'\">'+note+'</div><input type=\"number\" min=\"0\" max=\"127\" value=\"'+note+'\" oninput=\"document.getElementById(\\'btn-val-'+i+'\\').innerText=this.value\">';grid.appendChild(card);});});}";
    html += "function saveButtons(){const notes=[];for(let i=0;i<16;i++){notes.push(parseInt(document.getElementById('btn-val-'+i).innerText));}";
    html += "fetch('/api/buttons',{method:'POST',headers:{'Content-Type':'application/json'},body:JSON.stringify({channel:parseInt(document.getElementById('btn-midi-ch').value),mixerNote:parseInt(document.getElementById('mixer-btn-note').value),notes:notes})})";
    html += ".then(r=>r.json()).then(data=>{showStatus(data.message);});}";
    
    html += "function loadTrackActions(){fetch('/api/track-actions').then(r=>r.json()).then(data=>{const grid=document.getElementById('track-grid');grid.innerHTML='';";
    html += "data.layerLabels.forEach((label,l)=>{const row=document.createElement('div');row.className='card';row.style='grid-column:span 4;background:#1a1a1a;border:1px solid #444;text-align:left;padding:15px;display:flex;align-items:center;gap:15px;flex-wrap:wrap';";
    html += "const la=data.layerActions[l];";
    html += "row.innerHTML='<h2>Layer '+(l+1)+':</h2><input type=\"text\" id=\"track-layer-label-'+l+'\" value=\"'+escapeHtml(label)+'\" placeholder=\"Layer Name\" style=\"flex-grow:1;max-width:200px;margin:0\">';";
    html += "row.innerHTML+='<select id=\"track-layer-type-'+l+'\" style=\"width:100px;padding:8px;background:#333;color:white;border:1px solid #555;border-radius:4px;margin:0\"><option value=\"none\" '+(la.type===0?'selected':'')+'>None</option><option value=\"note\" '+(la.type===1?'selected':'')+'>Note</option><option value=\"cc\" '+(la.type===2?'selected':'')+'>CC</option></select>';";
    html += "row.innerHTML+='<div style=\"display:flex;gap:5px\"><input type=\"number\" id=\"track-layer-val-'+l+'\" min=\"0\" max=\"127\" value=\"'+la.value+'\" placeholder=\"Val\" style=\"width:60px;margin:0\"><input type=\"number\" id=\"track-layer-ch-'+l+'\" min=\"1\" max=\"16\" value=\"'+(la.channel+1)+'\" placeholder=\"CH\" style=\"width:50px;margin:0\"></div>';";
    html += "grid.appendChild(row);";
    html += "const layerActions=data.actions[l];layerActions.forEach((a,b)=>{const card=document.createElement('div');card.className='card';card.innerHTML='<h3>Btn '+(b+1)+'</h3><input type=\"text\" id=\"track-label-'+l+'-'+b+'\" value=\"'+escapeHtml(a.label)+'\" placeholder=\"Label\">';";
    html += "card.innerHTML+='<select id=\"track-type-'+l+'-'+b+'\" style=\"width:100%;padding:8px;background:#333;color:white;border:1px solid #555;border-radius:4px;margin-bottom:10px\"><option value=\"none\" '+(a.type===0?'selected':'')+'>None</option><option value=\"note\" '+(a.type===1?'selected':'')+'>Note</option><option value=\"cc\" '+(a.type===2?'selected':'')+'>CC</option></select>';";
    html += "card.innerHTML+='<div style=\"display:flex;gap:5px\"><input type=\"number\" id=\"track-val-'+l+'-'+b+'\" min=\"0\" max=\"127\" value=\"'+a.value+'\" placeholder=\"Val\"><input type=\"number\" id=\"track-ch-'+l+'-'+b+'\" min=\"1\" max=\"16\" value=\"'+(a.channel+1)+'\" placeholder=\"CH\"></div>';grid.appendChild(card);});});});}";
    html += "function saveTrackActions(){const layerLabels=[];const layerActions=[];for(let l=0;l<4;l++){layerLabels.push(document.getElementById('track-layer-label-'+l).value);const lTypeStr=document.getElementById('track-layer-type-'+l).value;let lTypeIdx=0;if(lTypeStr==='note')lTypeIdx=1;else if(lTypeStr==='cc')lTypeIdx=2;layerActions.push({type:lTypeIdx,value:parseInt(document.getElementById('track-layer-val-'+l).value),channel:parseInt(document.getElementById('track-layer-ch-'+l).value)-1});}const actions=[];for(let l=0;l<4;l++){const subActions=[];for(let b=0;b<4;b++){const typeStr=document.getElementById('track-type-'+l+'-'+b).value;let typeIdx=0;if(typeStr==='note')typeIdx=1;else if(typeStr==='cc')typeIdx=2;subActions.push({type:typeIdx,value:parseInt(document.getElementById('track-val-'+l+'-'+b).value),channel:parseInt(document.getElementById('track-ch-'+l+'-'+b).value)-1,label:document.getElementById('track-label-'+l+'-'+b).value});}actions.push(subActions);}";
    html += "fetch('/api/track-actions',{method:'POST',headers:{'Content-Type':'application/json'},body:JSON.stringify({layerLabels:layerLabels,layerActions:layerActions,actions:actions})}).then(r=>r.json()).then(data=>{showStatus(data.message);});}";

    html += "function loadTemplateActions(){fetch('/api/template-actions').then(r=>r.json()).then(data=>{const grid=document.getElementById('template-grid');grid.innerHTML='';";
    html += "for(let i=0;i<8;i++){const card=document.createElement('div');card.className='card';";
    html += "let h3Text = i < 4 ? ('Row 1, Btn '+(i+1)) : ('Row 2, Btn '+(i-3));";
    html += "card.innerHTML='<h3>'+h3Text+'</h3><input type=\"text\" id=\"template-label-'+i+'\" value=\"'+escapeHtml(data.labels[i])+'\" placeholder=\"Label\">';";
    html += "card.innerHTML+='<div style=\"display:flex;gap:5px\"><input type=\"number\" id=\"template-note-'+i+'\" min=\"0\" max=\"127\" value=\"'+data.notes[i]+'\" placeholder=\"Note\"></div>';grid.appendChild(card);}";
    html += "document.getElementById('template-pin12-note').value=data.pin12Note;document.getElementById('template-pin14-note').value=data.pin14Note;";
    html += "});}";
    html += "function saveTemplateActions(){const labels=[];const notes=[];for(let i=0;i<8;i++){labels.push(document.getElementById('template-label-'+i).value);notes.push(parseInt(document.getElementById('template-note-'+i).value));}";
    html += "const pin12=parseInt(document.getElementById('template-pin12-note').value);const pin14=parseInt(document.getElementById('template-pin14-note').value);";
    html += "fetch('/api/template-actions',{method:'POST',headers:{'Content-Type':'application/json'},body:JSON.stringify({labels:labels,notes:notes,pin12Note:pin12,pin14Note:pin14})}).then(r=>r.json()).then(data=>{showStatus(data.message);});}";


    html += "let page1Labels=[]; function loadMixerConfig(){const pageSelect = document.getElementById('mixer-page-select'); const page=parseInt(pageSelect?.value||0);";
    html += "fetch('/api/mixer-config?page='+page).then(r=>r.json()).then(data=>{page1Labels=data.page1Labels; document.getElementById('mixer-btn-label').value=data.buttonLabel;const grid=document.getElementById('mixer-grid');grid.innerHTML='';";
    html += "data.arcs.forEach((a,i)=>{const card=document.createElement('div');card.className='card';card.innerHTML='<h3>Arc '+(i+1)+'</h3><textarea id=\"mixer-arc-label-'+i+'\" placeholder=\"Label\">'+escapeHtml(a.label)+'</textarea>';";
    html += "card.innerHTML+='<div style=\"display:flex;gap:5px\"><input type=\"number\" id=\"mixer-arc-cc-'+i+'\" min=\"1\" max=\"127\" value=\"'+a.cc+'\" placeholder=\"CC\" oninput=\"lookupMixerLabel('+i+')\"><input type=\"number\" id=\"mixer-arc-ch-'+i+'\" min=\"1\" max=\"16\" value=\"'+(a.channel+1)+'\" placeholder=\"CH\" oninput=\"lookupMixerLabel('+i+')\"></div>';grid.appendChild(card);});});}";
    html += "function lookupMixerLabel(i){const cc=parseInt(document.getElementById('mixer-arc-cc-'+i).value);const ch=parseInt(document.getElementById('mixer-arc-ch-'+i).value)-1;";
    html += "if(page1Labels[ch]&&cc>=1&&cc<=16){const label=page1Labels[ch][cc-1]; if(label)document.getElementById('mixer-arc-label-'+i).value=label;}}";
    html += "function saveMixerConfig(){const page=parseInt(document.getElementById('mixer-page-select').value);const arcs=[];for(let i=0;i<16;i++){arcs.push({cc:parseInt(document.getElementById('mixer-arc-cc-'+i).value),channel:parseInt(document.getElementById('mixer-arc-ch-'+i).value)-1,label:document.getElementById('mixer-arc-label-'+i).value});}";
    html += "fetch('/api/mixer-config',{method:'POST',headers:{'Content-Type':'application/json'},body:JSON.stringify({page:page,buttonLabel:document.getElementById('mixer-btn-label').value,arcs:arcs})})";
    html += ".then(r=>r.json()).then(data=>{showStatus(data.message); if(data.message.includes('updated')) { ";
    html += "const label=document.getElementById('mixer-btn-label').value;";
    html += "const opt=document.getElementById('mixer-page-select').options[page]; if(opt) opt.text=label; } });}";
    
    html += "function loadFiles(){fetch('/api/files').then(r=>r.json()).then(data=>{const list=document.getElementById('file-list');list.innerHTML='';";
    html += "data.forEach(f=>{const row=document.createElement('div');row.className='card';row.style='display:flex;justify-content:space-between;align-items:center;padding:10px;margin-bottom:5px';";
    html += "row.innerHTML='<div><strong>'+f.name+'</strong> <small>('+(f.size/1024).toFixed(1)+' KB)</small></div><div><button class=\"tab-btn\" style=\"padding:5px 10px;margin:0 5px\" onclick=\"downloadFile(\\''+f.name+'\\')\">⬇️</button><button class=\"tab-btn\" style=\"padding:5px 10px;margin:0;background:#f44336\" onclick=\"deleteFile(\\''+f.name+'\\')\">🗑️</button></div>';list.appendChild(row);});});}";
    html += "function downloadFile(n){window.location.href='/api/download?name='+n;}";
    html += "function deleteFile(n){if(confirm('Delete '+n+'?')){fetch('/api/files?name='+n,{method:'DELETE'}).then(r=>r.json()).then(data=>{showStatus(data.message||data.error);loadFiles();});}}";
    html += "function uploadFile(){const fileInput=document.getElementById('file-input');if(fileInput.files.length===0)return;const formData=new FormData();formData.append('file',fileInput.files[0],fileInput.files[0].name);";
    html += "fetch('/api/upload',{method:'POST',body:formData}).then(r=>{showStatus('Upload complete');fileInput.value='';loadFiles();});}";
    
    html += "function showStatus(msg){document.getElementById('status').innerHTML='<span style=\"color:#4CAF50\">✓ '+msg+'</span>';setTimeout(()=>document.getElementById('status').innerHTML='',3000);}";
    html += "function updateValue(index,value){document.getElementById('value-'+index).innerText=value;}";
    html += "function saveConfig(){const config=[];const cards=document.querySelectorAll('#grid .card');cards.forEach((card,i)=>{config.push({value:parseInt(document.getElementById('value-'+i)?.innerText||0),label:document.getElementById('label-'+i)?.value||'',detent:document.getElementById('detent-check-'+i)?.checked?1:0,mode:parseInt(document.getElementById('mode-'+i)?.value||0)});});";
    html += "fetch('/api/config',{method:'POST',headers:{'Content-Type':'application/json'},body:JSON.stringify({page:currentPage,channel:currentChannel,pots:config})})";
    html += ".then(r=>r.json()).then(data=>{showStatus(data.message);});}";
    html += "function updateActiveStates(page,channel){document.querySelectorAll('.page-btn').forEach(btn=>btn.classList.remove('active'));document.querySelectorAll('.channel-btn').forEach(btn=>btn.classList.remove('active'));";
    html += "document.querySelector(\".page-btn[data-page='\"+page+\"']\")?.classList.add('active');document.querySelector(\".channel-btn[data-channel='\"+channel+\"']\")?.classList.add('active');}";
    html += "function escapeHtml(text){const div=document.createElement('div');div.textContent=text;return div.innerHTML;}loadPage(0,0);</script></head><body><div class='container'>";
    html += "<div class='header'><h1><img src='/wipidim.gif' alt='Logo' style='height:1em; vertical-align:middle; margin-right:10px;'>2048CC MIDI Configuration</h1></div>";
    html += "<div class='tab-nav'><button class='tab-btn active' onclick=\"showTab('pots')\">Potentiometers</button><button class='tab-btn' onclick=\"showTab('buttons')\">Channel Buttons</button><button class='tab-btn' onclick=\"showTab('track')\">Track Actions</button><button class='tab-btn' onclick=\"showTab('template')\">Template</button><button class='tab-btn' onclick=\"showTab('mixer')\">Mixer Pages</button><button class='tab-btn' onclick=\"showTab('files')\">SD Files</button></div>";
    html += "<div id='pots-tab' class='tab-content'><div class='channel-bar' style='justify-content:center'>";
    for (int i = 0; i < 16; i++) html += "<button class='channel-btn' data-channel='" + String(i) + "' onclick='loadPage(currentPage, " + String(i) + ")'>Ch " + String(i + 1) + "</button>";
    html += "</div><div class='nav'>";
    for (int i = 0; i < 8; i++) html += "<button class='page-btn' data-page='" + String(i) + "' onclick='loadPage(" + String(i) + ", currentChannel)'>Page " + String(i + 1) + "</button>";
    html += "</div><div id='grid' class='grid'></div><div style='text-align:center'><button class='save-btn' onclick='saveConfig()'>💾 Save Pot Config</button></div></div>";
    
    html += "<div id='track-tab' class='tab-content hidden'><div id='track-grid' class='grid'></div><div style='text-align:center'><button class='save-btn' onclick='saveTrackActions()'>💾 Save Track Actions</button></div></div>";
    html += "<div id='template-tab' class='tab-content hidden'><div style='text-align:center;margin-bottom:15px'>Pin 12 Note: <input type='number' id='template-pin12-note' style='width:60px'> Pin 14 Note: <input type='number' id='template-pin14-note' style='width:60px'></div>";
    html += "<div id='template-grid' class='grid'></div><div style='text-align:center'><button class='save-btn' onclick='saveTemplateActions()'>💾 Save Template Actions</button></div></div>";

    html += "<div id='buttons-tab' class='tab-content hidden'><div style='display:flex;justify-content:center;gap:20px;margin-bottom:20px'>";
    html += "<div class='card' style='max-width:300px;margin:0'><h3>Global Button MIDI Channel</h3>";
    html += "<select id='btn-midi-ch' style='width:100%;padding:8px;background:#333;color:white;border:none;border-radius:4px'>";
    for (int i = 0; i < 16; i++) html += "<option value='" + String(i) + "'>Channel " + String(i + 1) + "</option>";
    html += "</select></div>";
    html += "<div class='card' style='max-width:300px;margin:0'><h3>Mixer Button Note</h3><input type='number' id='mixer-btn-note' min='0' max='127' style='width:100%;padding:8px;background:#333;color:white;border:none;border-radius:4px'></div>";
    html += "</div><div id='btn-grid' class='grid'></div><div style='text-align:center'><button class='save-btn' onclick='saveButtons()'>💾 Save Button Config</button></div></div>";
    html += "<div id='mixer-tab' class='tab-content hidden'><div style='display:flex;justify-content:center;gap:20px;margin-bottom:20px'>";
    html += "<div class='card' style='max-width:300px;margin:0'><h3>Configure Page</h3><select id='mixer-page-select' onchange='loadMixerConfig()' style='width:100%;padding:8px;background:#333;color:white;border:none;border-radius:4px'>";
    for (int i = 0; i < 5; i++) {
        html += "<option value='" + String(i) + "'>" + MidiManager::mixerButtonLabels[i] + "</option>";
    }
    html += "</select></div>";
    html += "<div class='card' style='max-width:300px;margin:0'><h3>Button Label</h3><input type='text' id='mixer-btn-label' style='width:100%;padding:8px;background:#333;color:white;border:none;border-radius:4px'></div>";
    html += "</div><div id='mixer-grid' class='grid'></div><div style='text-align:center'><button class='save-btn' onclick='saveMixerConfig()'>💾 Save Mixer Config</button></div></div>";
    html += "<div id='files-tab' class='tab-content hidden'><div class='card' style='max-width:600px;margin:0 auto 20px auto'><h3>Upload File</h3><div style='display:flex;gap:10px'><input type='file' id='file-input' style='flex-grow:1;padding:8px;background:#333;color:white;border:none;border-radius:4px'><button class='save-btn' style='margin:0;padding:8px 20px' onclick='uploadFile()'>📤 Upload</button></div></div><div id='file-list' style='max-width:600px;margin:0 auto'></div></div>";
    html += "<div id='status' class='status'></div></div></body></html>";
    return html;
}
