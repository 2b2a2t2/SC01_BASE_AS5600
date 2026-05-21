#include "UiManager.h"
#define LGFX_USE_V1
#include <LovyanGFX.hpp>
#include "src/UI/ui.h"
#include "src/UI/components/ui_comp.h"
#include "src/UI/components/ui_comp_containerpots.h"
#include "HardwareHAL.h"
#include "SensorManager.h"
#include "StorageManager.h"

// --- LGFX Implementation for WT32-SC01 Plus ---
class LGFX : public lgfx::LGFX_Device {
    lgfx::Panel_ST7796 _panel_instance;
    lgfx::Bus_Parallel8 _bus_instance;
    lgfx::Light_PWM _light_instance;
    lgfx::Touch_FT5x06 _touch_instance;

public:
    LGFX(void) {
        {
            auto cfg = _bus_instance.config();
            cfg.freq_write = 40000000;
            cfg.pin_wr = 47;
            cfg.pin_rd = -1;
            cfg.pin_rs = 0;
            cfg.pin_d0 = 9;
            cfg.pin_d1 = 46;
            cfg.pin_d2 = 3;
            cfg.pin_d3 = 8;
            cfg.pin_d4 = 18;
            cfg.pin_d5 = 17;
            cfg.pin_d6 = 16;
            cfg.pin_d7 = 15;
            _bus_instance.config(cfg);
            _panel_instance.setBus(&_bus_instance);
        }
        {
            auto cfg = _panel_instance.config();
            cfg.pin_cs = -1; cfg.pin_rst = 4; cfg.pin_busy = -1;
            cfg.memory_width = 320; cfg.memory_height = 480;
            cfg.panel_width = 320; cfg.panel_height = 480;
            cfg.offset_x = 0; cfg.offset_y = 0; cfg.offset_rotation = 0;
            cfg.dummy_read_pixel = 8; cfg.dummy_read_bits = 1;
            cfg.readable = true; cfg.invert = true; cfg.rgb_order = false;
            cfg.dlen_16bit = false; cfg.bus_shared = true;
            _panel_instance.config(cfg);
        }
        {
            auto cfg = _light_instance.config();
            cfg.pin_bl = 45; cfg.invert = false; cfg.freq = 44100; cfg.pwm_channel = 7;
            _light_instance.config(cfg);
            _panel_instance.setLight(&_light_instance);
        }
        {
            auto cfg = _touch_instance.config();
            cfg.i2c_port = 1; cfg.i2c_addr = 0x38;
            cfg.pin_sda = 6; cfg.pin_scl = 5;
            cfg.freq = 400000;
            cfg.x_min = 0; cfg.x_max = 320; cfg.y_min = 0; cfg.y_max = 480;
            _touch_instance.config(cfg);
            _panel_instance.setTouch(&_touch_instance);
        }
        setPanel(&_panel_instance);
    }
};

// Static member definitions
LGFX* UiManager::tft = nullptr;
lv_disp_draw_buf_t UiManager::draw_buf;
lv_color_t* UiManager::buf = nullptr;
lv_obj_t* UiManager::lfoCanvas = nullptr;
lv_draw_line_dsc_t UiManager::main_line_dsc;

lv_obj_t* UiManager::ui_ButtonChannel[16];
lv_obj_t* UiManager::ui_ButtonPage[8];
lv_obj_t* UiManager::ui_ButtonArc[NUM_ARCS];
lv_obj_t* UiManager::ui_Arc[NUM_ARCS];
lv_obj_t* UiManager::ui_LabelValue[NUM_ARCS];
lv_obj_t* UiManager::ui_LabelName[NUM_ARCS];
lv_obj_t* UiManager::ui_ArcMod[NUM_ARCS];
int UiManager::activeArcIndex = 0;
int UiManager::activeEditPage = 0;
int UiManager::activeEditChannel = 0;
bool UiManager::isLfoMode = true;
bool UiManager::isMixerMode = false;
bool UiManager::isShiftActive = false;
bool UiManager::shiftWasUsed = false;
bool UiManager::previousMixerMode = false;
int UiManager::ccRowIndex = 0;
int UiManager::currentMixerPage = 0;
UiManager::MenuState UiManager::currentMenuState = UiManager::MENU_CHANNEL;

// Externs from main sketch (temporarily)

// Event Handler Externs
extern void ui_event_ButtonChannelGeneric(lv_event_t *e);
extern void ui_event_ButtonPageGeneric(lv_event_t *e);
extern void ui_event_ButtonArcGeneric(lv_event_t *e);
extern void ui_event_ButtonArcModulationSelect(lv_event_t *e);
extern void ui_event_ArcGeneric(lv_event_t *e);
extern void lfo1EventHandler(lv_event_t *e);
extern void lfo2EventHandler(lv_event_t *e);
extern void lfo3EventHandler(lv_event_t *e);
extern void lfo4EventHandler(lv_event_t *e);
extern void lfoMixEventHandler(lv_event_t *e);
extern void ui_event_ButtonWifi(lv_event_t *e);

extern void ui_event_MainMenuGeneric(lv_event_t *e);
extern void ui_event_TrackLayerGeneric(lv_event_t *e);
extern void ui_event_TrackTopGeneric(lv_event_t *e);
extern void ui_event_KeyboardGeneric(lv_event_t *e);

// --- LVGL Callbacks ---
static void my_disp_flush(lv_disp_drv_t *disp, const lv_area_t *area, lv_color_t *color_p) {
    uint32_t w = (area->x2 - area->x1 + 1);
    uint32_t h = (area->y2 - area->y1 + 1);
    if (UiManager::tft) {
        UiManager::tft->startWrite();
        UiManager::tft->setAddrWindow(area->x1, area->y1, w, h);
        UiManager::tft->writePixels((lgfx::rgb565_t *)&color_p->full, w * h);
        UiManager::tft->endWrite();
    }
    lv_disp_flush_ready(disp);
}

static void my_touchpad_read(lv_indev_drv_t *indev_driver, lv_indev_data_t *data) {
    uint16_t x, y;
    if (UiManager::tft && UiManager::tft->getTouch(&x, &y)) {
        data->state = LV_INDEV_STATE_PR;
        data->point.x = x;
        data->point.y = y;
    } else {
        data->state = LV_INDEV_STATE_REL;
    }
}

void UiManager::init() {
    tft = new LGFX();
    tft->begin();
    tft->setRotation(1);
    tft->setBrightness(255);
    tft->fillScreen(TFT_BLACK);

    lv_init();
    buf = (lv_color_t*)malloc(sizeof(lv_color_t) * SCREEN_WIDTH * 10);
    lv_disp_draw_buf_init(&draw_buf, buf, NULL, SCREEN_WIDTH * 10);

    static lv_disp_drv_t disp_drv;
    lv_disp_drv_init(&disp_drv);
    disp_drv.hor_res = SCREEN_WIDTH;
    disp_drv.ver_res = SCREEN_HEIGHT;
    disp_drv.flush_cb = my_disp_flush;
    disp_drv.draw_buf = &draw_buf;
    lv_disp_drv_register(&disp_drv);

    static lv_indev_drv_t indev_drv;
    lv_indev_drv_init(&indev_drv);
    indev_drv.type = LV_INDEV_TYPE_POINTER;
    indev_drv.read_cb = my_touchpad_read;
    lv_indev_drv_register(&indev_drv);

    ui_init();
    initArcArrays();
    initButtons();
    setupMainLFO();

    // Attach Event Handlers
    for (int i = 0; i < NUM_ARCS; i++) {
        lv_obj_add_event_cb(ui_ButtonArc[i], ui_event_ButtonArcGeneric, LV_EVENT_CLICKED, NULL);
        lv_obj_add_event_cb(ui_ButtonArc[i], ui_event_ButtonArcModulationSelect, LV_EVENT_LONG_PRESSED, NULL);
        lv_obj_add_event_cb(ui_Arc[i], ui_event_ArcGeneric, LV_EVENT_VALUE_CHANGED, NULL);
    }

    for (int i = 0; i < 16; i++) {
        lv_obj_add_event_cb(ui_ButtonChannel[i], ui_event_ButtonChannelGeneric, LV_EVENT_ALL, NULL);
    }

    for (int i = 0; i < 8; i++) {
        lv_obj_add_event_cb(ui_ButtonPage[i], ui_event_ButtonPageGeneric, LV_EVENT_CLICKED, NULL);
    }

    lv_obj_add_event_cb(ui_ButtonSettings1, lfo1EventHandler, LV_EVENT_CLICKED, NULL);
    lv_obj_add_event_cb(ui_ButtonSettings2, lfo2EventHandler, LV_EVENT_CLICKED, NULL);
    lv_obj_add_event_cb(ui_ButtonSettings3, lfo3EventHandler, LV_EVENT_CLICKED, NULL);
    lv_obj_add_event_cb(ui_ButtonSettings4, lfo4EventHandler, LV_EVENT_CLICKED, NULL);
    lv_obj_add_event_cb(ui_ButtonLFOMix, lfoMixEventHandler, LV_EVENT_ALL, NULL);

    if (ui_ButtonWifi) {
        lv_obj_add_event_cb(ui_ButtonWifi, ui_event_ButtonWifi, LV_EVENT_CLICKED, NULL);
    }

    if (ui_SwitchLFOCC) {
        lv_obj_add_event_cb(ui_SwitchLFOCC, [](lv_event_t *e) {
            UiManager::toggleLfoMode();
        }, LV_EVENT_VALUE_CHANGED, NULL);
    }

    if (ui_Keyboard1) {
        lv_keyboard_set_textarea(ui_Keyboard1, ui_TextArea1);
        lv_obj_add_event_cb(ui_Keyboard1, [](lv_event_t *e) {
            UiManager::handleKeyboardEvent(e);
        }, LV_EVENT_READY, NULL);
        lv_obj_add_event_cb(ui_Keyboard1, [](lv_event_t *e) {
             lv_obj_add_flag(ui_PanelKeyboard, LV_OBJ_FLAG_HIDDEN);
        }, LV_EVENT_CANCEL, NULL);
    }

    if (ui_SwitchKillLFO) {
        lv_obj_add_event_cb(ui_SwitchKillLFO, [](lv_event_t *e) {
            lv_obj_t* sw = lv_event_get_target(e);
            bool killing = lv_obj_has_state(sw, LV_STATE_CHECKED);
            LfoEngine::isKilled = killing;
            
            if (killing) {
                // Kill All: Save and set to 0
                for (int i = 0; i < 4; i++) {
                    LfoEngine::savedMixAmounts[i] = LfoEngine::mixAmounts[i];
                    LfoEngine::mixAmounts[i] = 0;
                    // Only update the physical pot values if we are currently looking at the mix amounts in LFO mode
                    if (isLfoMode && LfoEngine::mixMode) {
                        SensorManager::potentiometerValues[16 + i] = 0;
                        MidiManager::needsResync = true;
                    }
                }
            } else {
                // Restore All
                for (int i = 0; i < 4; i++) {
                    LfoEngine::mixAmounts[i] = LfoEngine::savedMixAmounts[i];
                    // Only update the physical pot values if we are currently looking at the mix amounts in LFO mode
                    if (isLfoMode && LfoEngine::mixMode) {
                        SensorManager::potentiometerValues[16 + i] = LfoEngine::mixAmounts[i] * 127.0f;
                        MidiManager::needsResync = true;
                    }
                }
            }
            UiManager::updateParameterLabels();
        }, LV_EVENT_VALUE_CHANGED, NULL);
    }

    if (ui_SwitchLFOCC) {
        if (isLfoMode) lv_obj_clear_state(ui_SwitchLFOCC, LV_STATE_CHECKED);
        else lv_obj_add_state(ui_SwitchLFOCC, LV_STATE_CHECKED);
    }

    // Main Menu Buttons
    lv_obj_add_event_cb(ui_ButtonGeneral1, ui_event_MainMenuGeneric, LV_EVENT_CLICKED, NULL);
    lv_obj_add_event_cb(ui_ButtonGeneral2, ui_event_MainMenuGeneric, LV_EVENT_CLICKED, NULL);
    lv_obj_add_event_cb(ui_ButtonGeneral3, ui_event_MainMenuGeneric, LV_EVENT_CLICKED, NULL);
    lv_obj_add_event_cb(ui_ButtonGeneral4, ui_event_MainMenuGeneric, LV_EVENT_CLICKED, NULL);
    lv_obj_set_user_data(ui_ButtonGeneral1, (void*)MENU_CHANNEL);
    lv_obj_set_user_data(ui_ButtonGeneral2, (void*)MENU_TRACK);
    lv_obj_set_user_data(ui_ButtonGeneral3, (void*)MENU_KEYBOARD);
    lv_obj_set_user_data(ui_ButtonGeneral4, (void*)MENU_TEMPLATES);

    if (ui_PanelStats) {
        lv_obj_add_event_cb(ui_PanelStats, [](lv_event_t *e) {
            HardwareHAL::vibrate(5);
        }, LV_EVENT_CLICKED, NULL);
    }

    // Track Layer Buttons
    lv_obj_add_event_cb(ui_ButtonLayer1, ui_event_TrackLayerGeneric, LV_EVENT_ALL, NULL);
    lv_obj_add_event_cb(ui_ButtonLayer2, ui_event_TrackLayerGeneric, LV_EVENT_ALL, NULL);
    lv_obj_add_event_cb(ui_ButtonLayer3, ui_event_TrackLayerGeneric, LV_EVENT_ALL, NULL);
    lv_obj_add_event_cb(ui_ButtonLayer4, ui_event_TrackLayerGeneric, LV_EVENT_ALL, NULL);
    lv_obj_set_user_data(ui_ButtonLayer1, (void*)0);
    lv_obj_set_user_data(ui_ButtonLayer2, (void*)1);
    lv_obj_set_user_data(ui_ButtonLayer3, (void*)2);
    lv_obj_set_user_data(ui_ButtonLayer4, (void*)3);

    // Track Top Buttons
    lv_obj_add_event_cb(ui_ButtonTop1, ui_event_TrackTopGeneric, LV_EVENT_ALL, NULL);
    lv_obj_add_event_cb(ui_ButtonTop2, ui_event_TrackTopGeneric, LV_EVENT_ALL, NULL);
    lv_obj_add_event_cb(ui_ButtonTop3, ui_event_TrackTopGeneric, LV_EVENT_ALL, NULL);
    lv_obj_add_event_cb(ui_ButtonTop4, ui_event_TrackTopGeneric, LV_EVENT_ALL, NULL);
    lv_obj_set_user_data(ui_ButtonTop1, (void*)0);
    lv_obj_set_user_data(ui_ButtonTop2, (void*)1);
    lv_obj_set_user_data(ui_ButtonTop3, (void*)2);
    lv_obj_set_user_data(ui_ButtonTop4, (void*)3);

    // Keyboard Buttons
    lv_obj_t* kbButtons[] = { ui_ButtonKeyboard1, ui_ButtonKeyboard2, ui_ButtonKeyboard3, ui_ButtonKeyboard4, 
                             ui_ButtonKeyboard5, ui_ButtonKeyboard6, ui_ButtonKeyboard7, ui_ButtonKeyboard8, 
                             ui_ButtonKeyboard9, ui_ButtonKeyboard10, ui_ButtonKeyboard11, ui_ButtonKeyboard12 };
    for (int i = 0; i < 12; i++) {
        lv_obj_set_style_bg_color(kbButtons[i], lv_color_hex(0x000000), LV_PART_MAIN | LV_STATE_DEFAULT);
        lv_obj_set_style_bg_opa(kbButtons[i], 255, LV_PART_MAIN | LV_STATE_DEFAULT);
        lv_obj_add_event_cb(kbButtons[i], ui_event_KeyboardGeneric, LV_EVENT_ALL, NULL);
        lv_obj_set_user_data(kbButtons[i], (void*)(intptr_t)i);
    }

    setMenuState(MENU_CHANNEL);
}

void UiManager::update() {
    lv_timer_handler();
}

void UiManager::initArcArrays() {
    for (int i = 0; i < NUM_ARCS; i++) {
        int base_idx = i * 5;
        ui_ButtonArc[i] = ui_comp_get_child(ui_ContainerPots1, UI_COMP_CONTAINERPOTS_BUTTONARC1 + base_idx);
        ui_Arc[i] = ui_comp_get_child(ui_ContainerPots1, UI_COMP_CONTAINERPOTS_BUTTONARC1_ARC1 + base_idx);
        ui_LabelValue[i] = ui_comp_get_child(ui_ContainerPots1, UI_COMP_CONTAINERPOTS_BUTTONARC1_ARC1_LABELVALUE1 + base_idx);
        ui_LabelName[i] = ui_comp_get_child(ui_ContainerPots1, UI_COMP_CONTAINERPOTS_BUTTONARC1_ARC1_LABELPOT1 + base_idx);
        ui_ArcMod[i] = ui_comp_get_child(ui_ContainerPots1, UI_COMP_CONTAINERPOTS_BUTTONARC1_ARC1_ARCMOD1 + base_idx);
        
        lv_obj_set_user_data(ui_ButtonArc[i], (void *)(intptr_t)(i + 1));
        lv_obj_set_user_data(ui_Arc[i], ui_LabelValue[i]);
        
        // Ensure ArcMod doesn't block the button click unless it's the active arc
        lv_obj_clear_flag(ui_ArcMod[i], LV_OBJ_FLAG_CLICKABLE);
    }
}

void UiManager::initButtons() {
    // This part is tricky because the button objects (ui_ButtonChannel1 etc.) 
    // are declared in ui.h as extern and defined in ui.c.
    ui_ButtonChannel[0] = ui_ButtonChannel1; ui_ButtonChannel[1] = ui_ButtonChannel2;
    ui_ButtonChannel[2] = ui_ButtonChannel3; ui_ButtonChannel[3] = ui_ButtonChannel4;
    ui_ButtonChannel[4] = ui_ButtonChannel5; ui_ButtonChannel[5] = ui_ButtonChannel6;
    ui_ButtonChannel[6] = ui_ButtonChannel7; ui_ButtonChannel[7] = ui_ButtonChannel8;
    ui_ButtonChannel[8] = ui_ButtonChannel9; ui_ButtonChannel[9] = ui_ButtonChannel10;
    ui_ButtonChannel[10] = ui_ButtonChannel11; ui_ButtonChannel[11] = ui_ButtonChannel12;
    ui_ButtonChannel[12] = ui_ButtonChannel13; ui_ButtonChannel[13] = ui_ButtonChannel14;
    ui_ButtonChannel[14] = ui_ButtonChannel15; ui_ButtonChannel[15] = ui_ButtonChannel16;

    ui_ButtonPage[0] = ui_ButtonPage1; ui_ButtonPage[1] = ui_ButtonPage2;
    ui_ButtonPage[2] = ui_ButtonPage3; ui_ButtonPage[3] = ui_ButtonPage4;
    ui_ButtonPage[4] = ui_ButtonPage5; ui_ButtonPage[5] = ui_ButtonPage6;
    ui_ButtonPage[6] = ui_ButtonPage7; ui_ButtonPage[7] = ui_ButtonPage8;

    for (int i = 0; i < 16; i++) {
        lv_obj_set_user_data(ui_ButtonChannel[i], (void *)(intptr_t)(i + 1));
    }
    for (int i = 0; i < 8; i++) {
        lv_obj_set_user_data(ui_ButtonPage[i], (void *)(intptr_t)(i + 1));
    }
}

void UiManager::loadValuesForCurrentState() {
    MidiManager::loadValuesForCurrentState();
    for (int i = 0; i < NUM_POTS; i++) {
        lv_obj_t* arc = getArc(i);
        lv_obj_t* labelVal = getLabelValue(i);
        lv_obj_t* labelName = getLabelName(i);

        int val = (int)SensorManager::currentPotentiometerValues[i];
        if (arc) {
            lv_arc_set_value(arc, val);
            if (i < 16 && !isMixerMode) {
                bool hasDetent = MidiManager::storedPotentiometerDetents[MidiManager::currentPage][MidiManager::currentMidiChannel][i];
                if (hasDetent) {
                    lv_arc_set_mode(arc, LV_ARC_MODE_SYMMETRICAL);
                } else {
                    lv_arc_set_mode(arc, LV_ARC_MODE_NORMAL);
                }
            } else {
                lv_arc_set_mode(arc, LV_ARC_MODE_NORMAL);
            }
        }
        if (labelVal) {
            if (i < 16 && !isMixerMode && MidiManager::storedPotentiometerDetents[MidiManager::currentPage][MidiManager::currentMidiChannel][i]) {
                String s = (val - 64 >= 0 ? "+" : "") + String(val - 64);
                lv_label_set_text(labelVal, s.c_str());
            } else {
                lv_label_set_text_fmt(labelVal, "%d", val);
            }
        }
        
        if (labelName) {
            if (isMixerMode && !isLfoMode && i < 16) {
                lv_label_set_text(labelName, MidiManager::getMixerLabel(currentMixerPage, i).c_str());
            } else if (!isLfoMode && i < 16) {
                String label = MidiManager::currentArcLabels[i];
                if (label.length() > 0 && !label.startsWith("CC ")) lv_label_set_text(labelName, label.c_str());
                else lv_label_set_text_fmt(labelName, "CC %d", (MidiManager::currentPage * 16) + i + 1);
            }
        }
    }
    // Immediately refresh arcs 17-20 with the correct values for the new state
    // This must happen in the SAME call to avoid a deferred "jump" effect
    updateParameterLabels();
    updateModulationUIColors();
}


void UiManager::refreshDisplay() {
    for (int i = 0; i < NUM_POTS; i++) {
        if (ui_Arc[i]) {
            lv_arc_set_value(ui_Arc[i], (int)SensorManager::currentPotentiometerValues[i]);
            lv_label_set_text_fmt(ui_LabelValue[i], "%d", (int)SensorManager::currentPotentiometerValues[i]);
        }
    }
}

void UiManager::refreshSinglePot(int potIndex) {
    lv_obj_t* arc = getArc(potIndex);
    lv_obj_t* labelVal = getLabelValue(potIndex);
    
    if (potIndex >= 16 && isLfoMode) {
        // Targeted LFO update
        if (LfoEngine::mixMode) {
            float val = LfoEngine::mixAmounts[potIndex - 16];
            if (arc) lv_arc_set_value(arc, (int)(val * 127));
            if (labelVal) lv_label_set_text_fmt(labelVal, "%d%%", (int)(val * 100));
        } else {
            auto& lfo = LfoEngine::lfos[LfoEngine::currentLfoIndex];
            if (potIndex == 16) {
                if (arc) lv_arc_set_value(arc, (int)(lfo.depth * 127));
                if (labelVal) lv_label_set_text(labelVal, String(lfo.depth * MAX_LFO_DEPTH, 1).c_str());
            } else if (potIndex == 17) {
                if (arc) lv_arc_set_value(arc, (int)((lfo.rate / MAX_LFO_RATE) * 127));
                if (labelVal) lv_label_set_text(labelVal, String(lfo.rate, 1).c_str());
            } else if (potIndex == 18) {
                const char *shapes[] = { "Sine", "Tri", "Up", "Down", "Sqr", "Rand", "S&H" };
                if (arc) lv_arc_set_value(arc, (int)((lfo.shape / 6.0) * 127));
                if (labelVal) lv_label_set_text(labelVal, shapes[lfo.shape]);
            } else if (potIndex == 19) {
                if (arc) lv_arc_set_value(arc, (int)(lfo.offset * 127));
                if (labelVal) {
                    float dispVal = (lfo.offset - 0.5f) * 2.0f;
                    String s = (dispVal >= 0.005f ? "+" : "") + String(dispVal, 2);
                    lv_label_set_text(labelVal, s.c_str());
                }
            }
        }
    } else {
        // Standard CC/Mixer update
        int val = (int)SensorManager::currentPotentiometerValues[potIndex];
        if (arc) {
            lv_arc_set_value(arc, val);
            if (potIndex < 16 && !isMixerMode) {
                bool hasDetent = MidiManager::storedPotentiometerDetents[MidiManager::currentPage][MidiManager::currentMidiChannel][potIndex];
                if (hasDetent) {
                    lv_arc_set_mode(arc, LV_ARC_MODE_SYMMETRICAL);
                } else {
                    lv_arc_set_mode(arc, LV_ARC_MODE_NORMAL);
                }
            } else {
                lv_arc_set_mode(arc, LV_ARC_MODE_NORMAL);
            }
        }
        if (labelVal) {
            if (potIndex < 16 && !isMixerMode && MidiManager::storedPotentiometerDetents[MidiManager::currentPage][MidiManager::currentMidiChannel][potIndex]) {
                String s = (val - 64 >= 0 ? "+" : "") + String(val - 64);
                lv_label_set_text(labelVal, s.c_str());
            } else {
                lv_label_set_text_fmt(labelVal, "%d", val);
            }
        }
    }
}

void UiManager::setActiveArc(int index) {
    for (int i = 0; i < NUM_ARCS; i++) {
        lv_obj_clear_state(ui_ButtonArc[i], LV_STATE_CHECKED);
        lv_obj_clear_flag(ui_ArcMod[i], LV_OBJ_FLAG_CLICKABLE);
    }

    int targetPage = MidiManager::currentPage;
    int targetChannel = MidiManager::currentMidiChannel;
    int actualIndex = index;

    // Resolve Mixer mapping if applicable
    if (index <= 16 && isMixerMode) {
        uint8_t cc = MidiManager::mixerPageCCs[currentMixerPage][index - 1];
        uint8_t ch = MidiManager::mixerPageChannels[currentMixerPage][index - 1];
        if (cc >= 1 && cc <= 128) {
            actualIndex = ((cc - 1) % 16) + 1;
            targetPage = (cc - 1) / 16;
            targetChannel = ch;
        }
    } else if (index > 16) {
        if (isLfoMode) return; // Cannot edit labels in LFO mode for these pots
        actualIndex = (ccRowIndex * 4) + (index - 17) + 1;
        targetPage = 0; // Always Page 1 for mirrored arcs
        // targetChannel remains currentMidiChannel
    }

    if (actualIndex >= 1 && actualIndex <= NUM_ARCS) {
        activeArcIndex = actualIndex - 1;
        activeEditPage = targetPage;
        activeEditChannel = targetChannel;
        
        // If it's a standard arc, highlight it
        if (index <= 16) {
            lv_obj_add_state(ui_ButtonArc[index - 1], LV_STATE_CHECKED); // Use original index for highlighting
            lv_obj_add_flag(ui_ArcMod[index - 1], LV_OBJ_FLAG_CLICKABLE);
        }
        
        // Update TextArea and EditLabel with current label when selecting
        if (ui_TextArea1) {
            lv_textarea_set_text(ui_TextArea1, StorageManager::getLabel(activeEditPage, activeEditChannel, activeArcIndex).c_str());
        }
        
        if (ui_EditLabel) {
            lv_label_set_text_fmt(ui_EditLabel, "Channel : %d\nCC# : %d\nPage : %d        Position : %d", 
                                  MidiManager::currentMidiChannel + 1, 
                                  (targetPage * 16) + activeArcIndex + 1,
                                  targetPage + 1,
                                  activeArcIndex + 1);
        }
    }
}

void UiManager::setModulationTarget(int index) {
    if (index >= 1 && index <= NUM_ARCS) {
        int targetPage = MidiManager::currentPage;
        int targetChannel = MidiManager::currentMidiChannel;
        int targetArc = index;

        if (index <= 16 && isMixerMode) {
            uint8_t cc = MidiManager::mixerPageCCs[currentMixerPage][index - 1];
            uint8_t ch = MidiManager::mixerPageChannels[currentMixerPage][index - 1];
            if (cc >= 1 && cc <= 128) {
                targetArc = ((cc - 1) % 16) + 1;
                targetPage = (cc - 1) / 16;
                targetChannel = ch;
            }
        }

        MidiManager::selectedArcForModulation = targetArc;
        MidiManager::modulationTargetPage = targetPage;
        MidiManager::modulationTargetChannel = targetChannel;
        updateModulationUIColors();
        updatePageButtonColors();
        updateChannelButtonColors();

        // Flash the selected arc to confirm
        if (ui_ButtonArc[index - 1]) {
            lv_obj_set_style_bg_color(ui_ButtonArc[index - 1], lv_color_hex(0x94AA00), LV_PART_MAIN | LV_STATE_DEFAULT);
            
            struct TimerData {
                int idx;
                lv_obj_t* btn;
            };
            TimerData* data = new TimerData{index - 1, ui_ButtonArc[index - 1]};
            
            lv_timer_create([](lv_timer_t *t) {
                TimerData* d = (TimerData*)t->user_data;
                if (d->btn) {
                    lv_obj_set_style_bg_color(d->btn, lv_color_hex(0x000000), LV_PART_MAIN | LV_STATE_DEFAULT);
                }
                delete d;
                lv_timer_del(t);
            }, 200, data);
        }
    }
}

void UiManager::updateChannelButtonColors() {
    for (int i = 0; i < 16; i++) {
        // Background for Selected Channel
        if (i == MidiManager::currentMidiChannel) {
            lv_obj_set_style_bg_color(ui_ButtonChannel[i], lv_color_hex(0x184873), LV_PART_MAIN | LV_STATE_DEFAULT);
        } else {
            lv_obj_set_style_bg_color(ui_ButtonChannel[i], lv_color_hex(0x000000), LV_PART_MAIN | LV_STATE_DEFAULT);
        }

        // Outline for Active LFO Modulation Target Channel
        if (i == MidiManager::modulationTargetChannel) {
            lv_obj_set_style_outline_color(ui_ButtonChannel[i], lv_color_hex(0x94AA00), LV_PART_MAIN | LV_STATE_DEFAULT);
            lv_obj_set_style_outline_width(ui_ButtonChannel[i], 1, LV_PART_MAIN | LV_STATE_DEFAULT);
            lv_obj_set_style_outline_opa(ui_ButtonChannel[i], 255, LV_PART_MAIN | LV_STATE_DEFAULT);
        } else {
            lv_obj_set_style_outline_width(ui_ButtonChannel[i], 0, LV_PART_MAIN | LV_STATE_DEFAULT);
        }
    }
}

void UiManager::updatePageButtonColors() {
    for (int i = 0; i < 8; i++) {
        // Background for Selected Page
        if (i == MidiManager::currentPage) {
            lv_obj_set_style_bg_color(ui_ButtonPage[i], lv_color_hex(0x184873), LV_PART_MAIN | LV_STATE_DEFAULT);
        } else {
            lv_obj_set_style_bg_color(ui_ButtonPage[i], lv_color_hex(0x000000), LV_PART_MAIN | LV_STATE_DEFAULT);
        }

        // Outline for Active LFO Modulation Target Page
        if (i == MidiManager::modulationTargetPage) {
            lv_obj_set_style_outline_color(ui_ButtonPage[i], lv_color_hex(0x94AA00), LV_PART_MAIN | LV_STATE_DEFAULT);
            lv_obj_set_style_outline_width(ui_ButtonPage[i], 1, LV_PART_MAIN | LV_STATE_DEFAULT);
            lv_obj_set_style_outline_opa(ui_ButtonPage[i], 255, LV_PART_MAIN | LV_STATE_DEFAULT);
        } else {
            lv_obj_set_style_outline_width(ui_ButtonPage[i], 0, LV_PART_MAIN | LV_STATE_DEFAULT);
        }
    }
}

void UiManager::updateLFOButtonColors() {
    uint8_t defaultOpa = isLfoMode ? 100 : 255;

    lv_obj_set_style_bg_color(ui_ButtonSettings1, lv_color_hex(0x000000), LV_PART_MAIN | LV_STATE_DEFAULT);
    lv_obj_set_style_bg_opa(ui_ButtonSettings1, defaultOpa, LV_PART_MAIN | LV_STATE_DEFAULT);
    lv_obj_set_style_bg_color(ui_ButtonSettings2, lv_color_hex(0x000000), LV_PART_MAIN | LV_STATE_DEFAULT);
    lv_obj_set_style_bg_opa(ui_ButtonSettings2, defaultOpa, LV_PART_MAIN | LV_STATE_DEFAULT);
    lv_obj_set_style_bg_color(ui_ButtonSettings3, lv_color_hex(0x000000), LV_PART_MAIN | LV_STATE_DEFAULT);
    lv_obj_set_style_bg_opa(ui_ButtonSettings3, defaultOpa, LV_PART_MAIN | LV_STATE_DEFAULT);
    lv_obj_set_style_bg_color(ui_ButtonSettings4, lv_color_hex(0x000000), LV_PART_MAIN | LV_STATE_DEFAULT);
    lv_obj_set_style_bg_opa(ui_ButtonSettings4, defaultOpa, LV_PART_MAIN | LV_STATE_DEFAULT);
    lv_obj_set_style_bg_color(ui_ButtonLFOMix, lv_color_hex(0x000000), LV_PART_MAIN | LV_STATE_DEFAULT);
    lv_obj_set_style_bg_opa(ui_ButtonLFOMix, 255, LV_PART_MAIN | LV_STATE_DEFAULT);

    // Reset and highlight General Buttons (with solid opaque styling)
    lv_obj_t* menuBtns[] = { ui_ButtonGeneral1, ui_ButtonGeneral2, ui_ButtonGeneral3, ui_ButtonGeneral4 };
    for (int i = 0; i < 4; i++) {
        if (i == (int)currentMenuState) {
            lv_obj_set_style_bg_color(menuBtns[i], lv_color_hex(0x184873), LV_PART_MAIN | LV_STATE_DEFAULT);
            lv_obj_set_style_bg_opa(menuBtns[i], 255, LV_PART_MAIN | LV_STATE_DEFAULT);
        } else {
            lv_obj_set_style_bg_color(menuBtns[i], lv_color_hex(0x000000), LV_PART_MAIN | LV_STATE_DEFAULT);
            lv_obj_set_style_bg_opa(menuBtns[i], 255, LV_PART_MAIN | LV_STATE_DEFAULT);
        }
    }

    lv_obj_set_style_text_color(ui_LabelButtonSettings1, lv_color_hex(0xFFFFFF), LV_PART_MAIN | LV_STATE_DEFAULT);
    lv_obj_set_style_text_color(ui_LabelButtonSettings2, lv_color_hex(0xFFFFFF), LV_PART_MAIN | LV_STATE_DEFAULT);
    lv_obj_set_style_text_color(ui_LabelButtonSettings3, lv_color_hex(0xFFFFFF), LV_PART_MAIN | LV_STATE_DEFAULT);
    lv_obj_set_style_text_color(ui_LabelButtonSettings4, lv_color_hex(0xFFFFFF), LV_PART_MAIN | LV_STATE_DEFAULT);
    lv_obj_set_style_text_color(ui_LabelButtonLFOMix, lv_color_hex(0xFFFFFF), LV_PART_MAIN | LV_STATE_DEFAULT);

    if (currentMenuState == MENU_KEYBOARD) {
        if (ui_LabelButtonLFOMix) lv_label_set_text(ui_LabelButtonLFOMix, "Keyboard");
        if (ui_LabelButtonSettings1) lv_label_set_text(ui_LabelButtonSettings1, "Oct -");
        if (ui_LabelButtonSettings2) lv_label_set_text(ui_LabelButtonSettings2, "Oct +");
        if (ui_LabelButtonSettings3) lv_label_set_text(ui_LabelButtonSettings3, "Oct Reset");
        if (ui_LabelButtonSettings4) lv_label_set_text(ui_LabelButtonSettings4, "");
        
        // Indicate current octave (highlight if not zero)
        if (MidiManager::keyboardOctave != 0) {
            lv_obj_set_style_bg_color(ui_ButtonSettings3, lv_color_hex(0x184873), LV_PART_MAIN | LV_STATE_DEFAULT);
            lv_obj_set_style_bg_opa(ui_ButtonSettings3, 255, LV_PART_MAIN | LV_STATE_DEFAULT);
        }
        return;
    }

    if (isLfoMode) {
        // LFO Mode Labels
        if (ui_LabelButtonLFOMix) lv_label_set_text(ui_LabelButtonLFOMix, "LFO Mix");
        if (ui_LabelButtonSettings1) lv_label_set_text(ui_LabelButtonSettings1, "LFO 1");
        if (ui_LabelButtonSettings2) lv_label_set_text(ui_LabelButtonSettings2, "LFO 2");
        if (ui_LabelButtonSettings3) lv_label_set_text(ui_LabelButtonSettings3, "LFO 3");
        if (ui_LabelButtonSettings4) lv_label_set_text(ui_LabelButtonSettings4, "LFO 4");

        // LFO Highlight (94AA00)
        if (LfoEngine::mixMode) {
            lv_obj_set_style_bg_color(ui_ButtonLFOMix, lv_color_hex(0x94AA00), LV_PART_MAIN | LV_STATE_DEFAULT);
            lv_obj_set_style_bg_opa(ui_ButtonLFOMix, 255, LV_PART_MAIN | LV_STATE_DEFAULT);
            lv_obj_set_style_text_color(ui_LabelButtonLFOMix, lv_color_hex(0x000000), LV_PART_MAIN | LV_STATE_DEFAULT);
        } else {
            if (LfoEngine::currentLfoIndex == 0) {
                lv_obj_set_style_bg_color(ui_ButtonSettings1, lv_color_hex(0x94AA00), LV_PART_MAIN | LV_STATE_DEFAULT);
                lv_obj_set_style_bg_opa(ui_ButtonSettings1, 100, LV_PART_MAIN | LV_STATE_DEFAULT);
                lv_obj_set_style_text_color(ui_LabelButtonSettings1, lv_color_hex(0x000000), LV_PART_MAIN | LV_STATE_DEFAULT);
            } else if (LfoEngine::currentLfoIndex == 1) {
                lv_obj_set_style_bg_color(ui_ButtonSettings2, lv_color_hex(0x94AA00), LV_PART_MAIN | LV_STATE_DEFAULT);
                lv_obj_set_style_bg_opa(ui_ButtonSettings2, 100, LV_PART_MAIN | LV_STATE_DEFAULT);
                lv_obj_set_style_text_color(ui_LabelButtonSettings2, lv_color_hex(0x000000), LV_PART_MAIN | LV_STATE_DEFAULT);
            } else if (LfoEngine::currentLfoIndex == 2) {
                lv_obj_set_style_bg_color(ui_ButtonSettings3, lv_color_hex(0x94AA00), LV_PART_MAIN | LV_STATE_DEFAULT);
                lv_obj_set_style_bg_opa(ui_ButtonSettings3, 100, LV_PART_MAIN | LV_STATE_DEFAULT);
                lv_obj_set_style_text_color(ui_LabelButtonSettings3, lv_color_hex(0x000000), LV_PART_MAIN | LV_STATE_DEFAULT);
            } else if (LfoEngine::currentLfoIndex == 3) {
                lv_obj_set_style_bg_color(ui_ButtonSettings4, lv_color_hex(0x94AA00), LV_PART_MAIN | LV_STATE_DEFAULT);
                lv_obj_set_style_bg_opa(ui_ButtonSettings4, 100, LV_PART_MAIN | LV_STATE_DEFAULT);
                lv_obj_set_style_text_color(ui_LabelButtonSettings4, lv_color_hex(0x000000), LV_PART_MAIN | LV_STATE_DEFAULT);
            }
        }
    } else {
        // CC/Mixer Mode Labels
        if (isMixerMode) {
            if (ui_LabelButtonLFOMix) lv_label_set_text(ui_LabelButtonLFOMix, MidiManager::mixerButtonLabels[0].c_str());
            if (ui_LabelButtonSettings1) lv_label_set_text(ui_LabelButtonSettings1, MidiManager::mixerButtonLabels[1].c_str());
            if (ui_LabelButtonSettings2) lv_label_set_text(ui_LabelButtonSettings2, MidiManager::mixerButtonLabels[2].c_str());
            if (ui_LabelButtonSettings3) lv_label_set_text(ui_LabelButtonSettings3, MidiManager::mixerButtonLabels[3].c_str());
            if (ui_LabelButtonSettings4) lv_label_set_text(ui_LabelButtonSettings4, MidiManager::mixerButtonLabels[4].c_str());

            // Mixer Highlight
            if (currentMixerPage == 0) {
                lv_obj_set_style_bg_color(ui_ButtonLFOMix, lv_color_hex(0x2196F3), LV_PART_MAIN | LV_STATE_DEFAULT);
                lv_obj_set_style_bg_opa(ui_ButtonLFOMix, 255, LV_PART_MAIN | LV_STATE_DEFAULT);
            } else if (currentMixerPage == 1) {
                lv_obj_set_style_bg_color(ui_ButtonSettings1, lv_color_hex(0x184873), LV_PART_MAIN | LV_STATE_DEFAULT);
                lv_obj_set_style_bg_opa(ui_ButtonSettings1, 255, LV_PART_MAIN | LV_STATE_DEFAULT);
            } else if (currentMixerPage == 2) {
                lv_obj_set_style_bg_color(ui_ButtonSettings2, lv_color_hex(0x184873), LV_PART_MAIN | LV_STATE_DEFAULT);
                lv_obj_set_style_bg_opa(ui_ButtonSettings2, 255, LV_PART_MAIN | LV_STATE_DEFAULT);
            } else if (currentMixerPage == 3) {
                lv_obj_set_style_bg_color(ui_ButtonSettings3, lv_color_hex(0x184873), LV_PART_MAIN | LV_STATE_DEFAULT);
                lv_obj_set_style_bg_opa(ui_ButtonSettings3, 255, LV_PART_MAIN | LV_STATE_DEFAULT);
            } else if (currentMixerPage == 4) {
                lv_obj_set_style_bg_color(ui_ButtonSettings4, lv_color_hex(0x184873), LV_PART_MAIN | LV_STATE_DEFAULT);
                lv_obj_set_style_bg_opa(ui_ButtonSettings4, 255, LV_PART_MAIN | LV_STATE_DEFAULT);
            }
        } else {
            if (ui_LabelButtonLFOMix) lv_label_set_text(ui_LabelButtonLFOMix, "Mixer");
            if (ui_LabelButtonSettings1) lv_label_set_text(ui_LabelButtonSettings1, "Row 1");
            if (ui_LabelButtonSettings2) lv_label_set_text(ui_LabelButtonSettings2, "Row 2");
            if (ui_LabelButtonSettings3) lv_label_set_text(ui_LabelButtonSettings3, "Row 3");
            if (ui_LabelButtonSettings4) lv_label_set_text(ui_LabelButtonSettings4, "Row 4");

            // CC Row Highlight
            if (ccRowIndex == 0) {
                lv_obj_set_style_bg_color(ui_ButtonSettings1, lv_color_hex(0x184873), LV_PART_MAIN | LV_STATE_DEFAULT);
                lv_obj_set_style_bg_opa(ui_ButtonSettings1, 255, LV_PART_MAIN | LV_STATE_DEFAULT);
            } else if (ccRowIndex == 1) {
                lv_obj_set_style_bg_color(ui_ButtonSettings2, lv_color_hex(0x184873), LV_PART_MAIN | LV_STATE_DEFAULT);
                lv_obj_set_style_bg_opa(ui_ButtonSettings2, 255, LV_PART_MAIN | LV_STATE_DEFAULT);
            } else if (ccRowIndex == 2) {
                lv_obj_set_style_bg_color(ui_ButtonSettings3, lv_color_hex(0x184873), LV_PART_MAIN | LV_STATE_DEFAULT);
                lv_obj_set_style_bg_opa(ui_ButtonSettings3, 255, LV_PART_MAIN | LV_STATE_DEFAULT);
            } else if (ccRowIndex == 3) {
                lv_obj_set_style_bg_color(ui_ButtonSettings4, lv_color_hex(0x184873), LV_PART_MAIN | LV_STATE_DEFAULT);
                lv_obj_set_style_bg_opa(ui_ButtonSettings4, 255, LV_PART_MAIN | LV_STATE_DEFAULT);
            }
        }
    }
}

void UiManager::updateModulationUIColors() {
    for (int i = 0; i < NUM_ARCS; i++) {
        if (ui_ArcMod[i]) {
            if (!isMixerMode || isLfoMode) {
                lv_obj_clear_flag(ui_ArcMod[i], LV_OBJ_FLAG_HIDDEN);
            }
            if (MidiManager::currentPage == MidiManager::modulationTargetPage && MidiManager::currentMidiChannel == MidiManager::modulationTargetChannel && i == (MidiManager::selectedArcForModulation - 1)) {
                lv_obj_set_style_arc_color(ui_ArcMod[i], lv_color_hex(0xFF6600), LV_PART_INDICATOR | LV_STATE_DEFAULT);
            } else {
                lv_obj_set_style_arc_color(ui_ArcMod[i], lv_color_hex(0xA1C9FB), LV_PART_INDICATOR | LV_STATE_DEFAULT);
                lv_arc_set_value(ui_ArcMod[i], 0);
            }
        }
    }
}

void UiManager::updateParameterLabels() {
    if (isLfoMode) {
        if (LfoEngine::mixMode) {
            lv_label_set_text_fmt(ui_LabelValue17, "%d%%", (int)(LfoEngine::mixAmounts[0] * 100));
            lv_label_set_text_fmt(ui_LabelValue18, "%d%%", (int)(LfoEngine::mixAmounts[1] * 100));
            lv_label_set_text_fmt(ui_LabelValue19, "%d%%", (int)(LfoEngine::mixAmounts[2] * 100));
            lv_label_set_text_fmt(ui_LabelValue20, "%d%%", (int)(LfoEngine::mixAmounts[3] * 100));
            lv_label_set_text(ui_LabelPot17, "Mix1");
            lv_label_set_text(ui_LabelPot18, "Mix2");
            lv_label_set_text(ui_LabelPot19, "Mix3");
            lv_label_set_text(ui_LabelPot20, "Mix4");

            lv_arc_set_value(ui_Arc17, (int)(LfoEngine::mixAmounts[0] * 127));
            lv_arc_set_value(ui_Arc18, (int)(LfoEngine::mixAmounts[1] * 127));
            lv_arc_set_value(ui_Arc19, (int)(LfoEngine::mixAmounts[2] * 127));
            lv_arc_set_value(ui_Arc20, (int)(LfoEngine::mixAmounts[3] * 127));
            if (ui_Arc20) {
                lv_arc_set_mode(ui_Arc20, LV_ARC_MODE_NORMAL);
                lv_obj_set_style_arc_opa(ui_Arc20, 255, LV_PART_INDICATOR | LV_STATE_DEFAULT);
            }
        } else {
            lv_label_set_text(ui_LabelValue17, String(LfoEngine::lfos[LfoEngine::currentLfoIndex].depth * MAX_LFO_DEPTH, 1).c_str());
            lv_label_set_text(ui_LabelValue18, String(LfoEngine::lfos[LfoEngine::currentLfoIndex].rate, 1).c_str());
            const char *shapes[] = { "Sine", "Tri", "Up", "Down", "Sqr", "Rand", "S&H" };
            lv_label_set_text(ui_LabelValue19, shapes[LfoEngine::lfos[LfoEngine::currentLfoIndex].shape]);
            {
                float dispVal = (LfoEngine::lfos[LfoEngine::currentLfoIndex].offset - 0.5f) * 2.0f;
                String s = (dispVal >= 0.005f ? "+" : "") + String(dispVal, 2);
                lv_label_set_text(ui_LabelValue20, s.c_str());
            }
            lv_label_set_text(ui_LabelPot17, "Depth");
            lv_label_set_text(ui_LabelPot18, "Rate");
            lv_label_set_text(ui_LabelPot19, "Shape");
            lv_label_set_text(ui_LabelPot20, "Offset");

            lv_arc_set_value(ui_Arc17, (int)(LfoEngine::lfos[LfoEngine::currentLfoIndex].depth * 127));
            lv_arc_set_value(ui_Arc18, (int)((LfoEngine::lfos[LfoEngine::currentLfoIndex].rate / MAX_LFO_RATE) * 127));
            lv_arc_set_value(ui_Arc19, (int)((LfoEngine::lfos[LfoEngine::currentLfoIndex].shape / 6.0) * 127));
            lv_arc_set_value(ui_Arc20, (int)(LfoEngine::lfos[LfoEngine::currentLfoIndex].offset * 127));
            if (ui_Arc20) {
                lv_arc_set_mode(ui_Arc20, LV_ARC_MODE_SYMMETRICAL);
                lv_obj_set_style_arc_opa(ui_Arc20, 255, LV_PART_INDICATOR | LV_STATE_DEFAULT);
            }
        }
    } else {
        if (ui_Arc20) {
            lv_arc_set_mode(ui_Arc20, LV_ARC_MODE_NORMAL);
            lv_obj_set_style_arc_opa(ui_Arc20, 255, LV_PART_INDICATOR | LV_STATE_DEFAULT);
        }
    }
    
    // ALWAYS update Arcs 1-16 (Top grid)
    if (isMixerMode) {
        // Mixer Page loading for top 16
        for (int i = 0; i < 16; i++) {
            int val = (int)SensorManager::potentiometerValues[i];
            lv_obj_t* arc = getArc(i);
            lv_obj_t* labelVal = getLabelValue(i);
            lv_obj_t* labelName = getLabelName(i);
            if (arc) lv_arc_set_value(arc, val);
            if (labelVal) lv_label_set_text_fmt(labelVal, "%d", val);
            if (labelName) {
                lv_label_set_text(labelName, MidiManager::getMixerLabel(currentMixerPage, i).c_str());
            }
        }
    } else {
        // Standard CC Page loading for top 16
        int baseCC = MidiManager::currentPage * 16;
        for (int i = 0; i < 16; i++) {
            int val = (int)SensorManager::potentiometerValues[i];
            lv_obj_t* arc = getArc(i);
            lv_obj_t* labelVal = getLabelValue(i);
            lv_obj_t* labelName = getLabelName(i);
            if (arc) lv_arc_set_value(arc, val);
            if (labelVal) lv_label_set_text_fmt(labelVal, "%d", val);
            if (labelName) {
                String label = MidiManager::currentArcLabels[i];
                if (label.length() > 0 && !label.startsWith("CC ")) lv_label_set_text(labelName, label.c_str());
                else lv_label_set_text_fmt(labelName, "CC %d", baseCC + i + 1);
            }
        }
    }

    // Update bottom arcs if NOT in LFO mode
    if (!isLfoMode) {
        if (isMixerMode) {
            int startCC = ccRowIndex * 4;
            for (int i = 0; i < 4; i++) {
                int potIdx = 16 + i;
                int ccVal = (int)SensorManager::potentiometerValues[potIdx];
                lv_obj_t* arc = getArc(potIdx);
                lv_obj_t* labelVal = getLabelValue(potIdx);
                lv_obj_t* labelName = getLabelName(potIdx);
                if (arc) lv_arc_set_value(arc, ccVal);
                if (labelVal) lv_label_set_text_fmt(labelVal, "%d", ccVal);
                if (labelName) {
                    String customLabel = StorageManager::getLabel(0, MidiManager::currentMidiChannel, startCC + i);
                    if (customLabel.length() > 0 && !customLabel.startsWith("CC ")) lv_label_set_text(labelName, customLabel.c_str());
                    else lv_label_set_text_fmt(labelName, "CC %d", startCC + i + 1);
                }
            }
        } else {
            // CC Mode - Arcs 17-20 control a row of CCs
            int startCC = ccRowIndex * 4;
            for (int i = 0; i < 4; i++) {
                int potIdx = 16 + i;
                int ccVal = (int)SensorManager::potentiometerValues[potIdx];
                
                lv_obj_t* arc = UiManager::getArc(potIdx);
                if (arc) lv_arc_set_value(arc, ccVal);
                
                lv_obj_t* labelVal = UiManager::getLabelValue(potIdx);
                if (labelVal) lv_label_set_text_fmt(labelVal, "%d", ccVal);
                
                lv_obj_t* labelName = getLabelName(potIdx);
                if (labelName) {
                    String customLabel = StorageManager::getLabel(0, MidiManager::currentMidiChannel, startCC + i);
                    if (customLabel.length() > 0 && !customLabel.startsWith("CC ")) {
                        lv_label_set_text(labelName, customLabel.c_str());
                    } else {
                        lv_label_set_text_fmt(labelName, "CC %d", startCC + i + 1);
                    }
                }
            }
        }
    }
}

void UiManager::updateSelectedArcMod(float mixedLFOValue) {
    if (!MidiManager::modulationEnabled || MidiManager::selectedArcForModulation < 1 || MidiManager::selectedArcForModulation > NUM_ARCS) {
        return;
    }

    if (isMixerMode && !isLfoMode) {
        // In Mixer mode, each arc 1-16 represents a channel volume (CC 7)
        // Only show modulation if the target is CC 7 (Page 1, Index 6) for that specific channel
        for (int i = 0; i < 16; i++) {
            if (ui_ArcMod[i]) {
                if (MidiManager::modulationTargetPage == 0 && MidiManager::selectedArcForModulation == 7 && MidiManager::modulationTargetChannel == i) {
                    lv_arc_set_value(ui_ArcMod[i], (int)mixedLFOValue);
                    lv_obj_clear_flag(ui_ArcMod[i], LV_OBJ_FLAG_HIDDEN);
                } else {
                    lv_obj_add_flag(ui_ArcMod[i], LV_OBJ_FLAG_HIDDEN);
                }
            }
        }
        return;
    }

    int arcIndex = MidiManager::selectedArcForModulation - 1;
    if (ui_ArcMod[arcIndex] && MidiManager::currentPage == MidiManager::modulationTargetPage && MidiManager::currentMidiChannel == MidiManager::modulationTargetChannel) {
        int lfoValue = (int)(mixedLFOValue * 127);
        lv_arc_set_value(ui_ArcMod[arcIndex], lfoValue);
    }
}

void UiManager::updateMainLFODisplay() {
    if (!lfoCanvas) return;

    lv_canvas_fill_bg(lfoCanvas, lv_color_hex(0x000000), LV_OPA_COVER);

    lv_point_t waveform_points[LfoEngine::MAIN_WAVEFORM_BUFFER_SIZE];

    for (int i = 0; i < LfoEngine::MAIN_WAVEFORM_BUFFER_SIZE; i++) {
        int idx = (LfoEngine::mainWaveformIndex + i) % LfoEngine::MAIN_WAVEFORM_BUFFER_SIZE;
        float value = LfoEngine::mainWaveformBuffer[idx];
        int x = 2 + (i * 100 / (LfoEngine::MAIN_WAVEFORM_BUFFER_SIZE - 1));
        float bipolar = (value - 0.5) * 2.0;
        int y = 32 - (int)(bipolar * 30);
        waveform_points[i].x = x;
        waveform_points[i].y = y;
    }

    lv_canvas_draw_line(lfoCanvas, waveform_points, LfoEngine::MAIN_WAVEFORM_BUFFER_SIZE, &main_line_dsc);

    lv_draw_line_dsc_t cursor_line_dsc;
    lv_draw_line_dsc_init(&cursor_line_dsc);
    cursor_line_dsc.color = lv_color_hex(0xFF0000);
    cursor_line_dsc.width = 1;

    lv_point_t cursor_points[2];
    cursor_points[0].x = 102; cursor_points[0].y = 2;
    cursor_points[1].x = 102; cursor_points[1].y = 62;
    lv_canvas_draw_line(lfoCanvas, cursor_points, 2, &cursor_line_dsc);

    lv_obj_invalidate(lfoCanvas);
}

void UiManager::setupMainLFO() {
    lfoCanvas = lv_canvas_create(ui_LFODisplay);
    lv_obj_set_size(lfoCanvas, 104, 64);
    lv_obj_align(lfoCanvas, LV_ALIGN_CENTER, 0, 0);

    static lv_color_t canvas_buf[104 * 64];
    lv_canvas_set_buffer(lfoCanvas, canvas_buf, 104, 64, LV_IMG_CF_TRUE_COLOR);

    lv_draw_line_dsc_init(&main_line_dsc);
    main_line_dsc.color = lv_color_hex(0x94AA00);
    main_line_dsc.width = 1;
    main_line_dsc.round_start = 1;
    main_line_dsc.round_end = 1;

    for (int i = 0; i < LfoEngine::MAIN_WAVEFORM_BUFFER_SIZE; i++) {
        LfoEngine::mainWaveformBuffer[i] = 0.5;
    }
}

lv_obj_t* UiManager::getArc(int index) {
    if (index >= 0 && index < 16) return ui_Arc[index];
    if (index == 16) return ui_Arc17;
    if (index == 17) return ui_Arc18;
    if (index == 18) return ui_Arc19;
    if (index == 19) return ui_Arc20;
    return nullptr;
}

lv_obj_t* UiManager::getLabelValue(int index) {
    if (index >= 0 && index < 16) return ui_LabelValue[index];
    if (index == 16) return ui_LabelValue17;
    if (index == 17) return ui_LabelValue18;
    if (index == 18) return ui_LabelValue19;
    if (index == 19) return ui_LabelValue20;
    return nullptr;
}

lv_obj_t* UiManager::getLabelName(int index) {
    if (index >= 0 && index < 16) return ui_LabelName[index];
    if (index == 16) return ui_LabelPot17;
    if (index == 17) return ui_LabelPot18;
    if (index == 18) return ui_LabelPot19;
    if (index == 19) return ui_LabelPot20;
    return nullptr;
}

void UiManager::handleKeyboardEvent(lv_event_t *e) {
    if (lv_event_get_code(e) == LV_EVENT_READY) {
        if (ui_TextArea1 && activeArcIndex >= 0 && activeArcIndex < NUM_ARCS) {
            const char* newLabel = lv_textarea_get_text(ui_TextArea1);
            StorageManager::saveLabel(activeEditPage, activeEditChannel, activeArcIndex, String(newLabel));
            
            // Update UI
            if (activeEditPage == MidiManager::currentPage && activeEditChannel == MidiManager::currentMidiChannel && ui_LabelName[activeArcIndex]) {
                lv_label_set_text(ui_LabelName[activeArcIndex], newLabel);
            }
            
            // Refresh mirrored labels if we are on Page 1 or editing a mirror
            updateParameterLabels();
            
            // Update the EditLabel to show context
            if (ui_EditLabel) {
                lv_label_set_text_fmt(ui_EditLabel, "Channel : %d\nCC# : %d\nPage : %d        Position : %d", 
                                      activeEditChannel + 1, 
                                      (activeEditPage * 16) + activeArcIndex + 1,
                                      activeEditPage + 1,
                                      activeArcIndex + 1);
            }
            
            // Hide keyboard panel
            lv_obj_add_flag(ui_PanelKeyboard, LV_OBJ_FLAG_HIDDEN);
            
            Serial.printf("UI: Label for arc %d updated to '%s' and saved.\n", activeArcIndex + 1, newLabel);
        }
    }
}

void UiManager::toggleLfoMode() {
    isLfoMode = !isLfoMode;
    
    if (ui_SwitchLFOCC) {
        if (isLfoMode) lv_obj_clear_state(ui_SwitchLFOCC, LV_STATE_CHECKED);
        else lv_obj_add_state(ui_SwitchLFOCC, LV_STATE_CHECKED);
    }
    
    if (isLfoMode) {
        if (ui_LabelButtonLFOMix) lv_label_set_text(ui_LabelButtonLFOMix, "LFO Mix");
        if (ui_LabelButtonSettings1) lv_label_set_text(ui_LabelButtonSettings1, "LFO 1");
        if (ui_LabelButtonSettings2) lv_label_set_text(ui_LabelButtonSettings2, "LFO 2");
        if (ui_LabelButtonSettings3) lv_label_set_text(ui_LabelButtonSettings3, "LFO 3");
        if (ui_LabelButtonSettings4) lv_label_set_text(ui_LabelButtonSettings4, "LFO 4");
        
        // Show LFO visuals
        if (ui_ContainerDisplay) lv_obj_clear_flag(ui_ContainerDisplay, LV_OBJ_FLAG_HIDDEN);
        if (ui_LFODisplay) lv_obj_clear_flag(ui_LFODisplay, LV_OBJ_FLAG_HIDDEN);
        if (ui_ButtonLFO1) lv_obj_clear_flag(ui_ButtonLFO1, LV_OBJ_FLAG_HIDDEN);
        if (ui_ButtonLFO2) lv_obj_clear_flag(ui_ButtonLFO2, LV_OBJ_FLAG_HIDDEN);
        if (ui_ButtonLFO3) lv_obj_clear_flag(ui_ButtonLFO3, LV_OBJ_FLAG_HIDDEN);
        if (ui_ButtonLFO4) lv_obj_clear_flag(ui_ButtonLFO4, LV_OBJ_FLAG_HIDDEN);
    } else {
        if (isMixerMode) {
            if (ui_LabelButtonLFOMix) lv_label_set_text(ui_LabelButtonLFOMix, MidiManager::mixerButtonLabels[0].c_str());
            if (ui_LabelButtonSettings1) lv_label_set_text(ui_LabelButtonSettings1, MidiManager::mixerButtonLabels[1].c_str());
            if (ui_LabelButtonSettings2) lv_label_set_text(ui_LabelButtonSettings2, MidiManager::mixerButtonLabels[2].c_str());
            if (ui_LabelButtonSettings3) lv_label_set_text(ui_LabelButtonSettings3, MidiManager::mixerButtonLabels[3].c_str());
            if (ui_LabelButtonSettings4) lv_label_set_text(ui_LabelButtonSettings4, MidiManager::mixerButtonLabels[4].c_str());
        } else {
            if (ui_LabelButtonLFOMix) lv_label_set_text(ui_LabelButtonLFOMix, "Mixer");
            if (ui_LabelButtonSettings1) lv_label_set_text(ui_LabelButtonSettings1, "Row 1");
            if (ui_LabelButtonSettings2) lv_label_set_text(ui_LabelButtonSettings2, "Row 2");
            if (ui_LabelButtonSettings3) lv_label_set_text(ui_LabelButtonSettings3, "Row 3");
            if (ui_LabelButtonSettings4) lv_label_set_text(ui_LabelButtonSettings4, "Row 4");
        }
        
        // Hide LFO visuals
        if (ui_ContainerDisplay) lv_obj_add_flag(ui_ContainerDisplay, LV_OBJ_FLAG_HIDDEN);
        if (ui_LFODisplay) lv_obj_add_flag(ui_LFODisplay, LV_OBJ_FLAG_HIDDEN);
        if (ui_ButtonLFO1) lv_obj_add_flag(ui_ButtonLFO1, LV_OBJ_FLAG_HIDDEN);
        if (ui_ButtonLFO2) lv_obj_add_flag(ui_ButtonLFO2, LV_OBJ_FLAG_HIDDEN);
        if (ui_ButtonLFO3) lv_obj_add_flag(ui_ButtonLFO3, LV_OBJ_FLAG_HIDDEN);
        if (ui_ButtonLFO4) lv_obj_add_flag(ui_ButtonLFO4, LV_OBJ_FLAG_HIDDEN);
    }
    
    updateParameterLabels();
    updateLFOButtonColors();
    loadValuesForCurrentState();
}

void UiManager::toggleMixerMode() {
    if (isLfoMode) return; // Only in CC mode
    
    isMixerMode = !isMixerMode;
    
    if (isMixerMode) {
        currentMixerPage = 0; // Default to first mixer page
        if (ui_LabelButtonLFOMix) lv_label_set_text(ui_LabelButtonLFOMix, MidiManager::mixerButtonLabels[0].c_str());
        if (ui_LabelButtonSettings1) lv_label_set_text(ui_LabelButtonSettings1, MidiManager::mixerButtonLabels[1].c_str());
        if (ui_LabelButtonSettings2) lv_label_set_text(ui_LabelButtonSettings2, MidiManager::mixerButtonLabels[2].c_str());
        if (ui_LabelButtonSettings3) lv_label_set_text(ui_LabelButtonSettings3, MidiManager::mixerButtonLabels[3].c_str());
        if (ui_LabelButtonSettings4) lv_label_set_text(ui_LabelButtonSettings4, MidiManager::mixerButtonLabels[4].c_str());
        
        loadValuesForCurrentState();
    } else {
        // Restore standard labels (LFO/Row #)
        if (ui_LabelButtonLFOMix) lv_label_set_text(ui_LabelButtonLFOMix, "Mixer");
        if (ui_LabelButtonSettings1) lv_label_set_text(ui_LabelButtonSettings1, "Row 1");
        if (ui_LabelButtonSettings2) lv_label_set_text(ui_LabelButtonSettings2, "Row 2");
        if (ui_LabelButtonSettings3) lv_label_set_text(ui_LabelButtonSettings3, "Row 3");
        if (ui_LabelButtonSettings4) lv_label_set_text(ui_LabelButtonSettings4, "Row 4");
        
        loadValuesForCurrentState();
    }
    
    updateParameterLabels();
    updateLFOButtonColors();
}

void UiManager::setCcRow(int row) {
    if (isLfoMode) return;
    if (isMixerMode) {
        currentMixerPage = row + 1; // Settings 1-4 map to Pages 2-5 (index 1-4)
    } else {
        ccRowIndex = row;
        MidiManager::channelRows[MidiManager::currentMidiChannel] = ccRowIndex;
    }
    loadValuesForCurrentState();
    updateParameterLabels();
    updateLFOButtonColors();
}
void UiManager::setMixerPage(int page) {
    if (page >= 0 && page < 5) {
        currentMixerPage = page;
        isMixerMode = true;
        shiftWasUsed = true; // Mark that we used the shift for a selection
        updateParameterLabels();
        updateLFOButtonColors();
        loadValuesForCurrentState();
    }
}

void UiManager::syncLfoArcValues() {
    if (!isLfoMode) return;
    
    // Reload potentiometerValues[16-19] from current LFO state so there's no jump
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
    
    // Re-baseline sensor angles for pots 16-19 only to prevent jumps
    MidiManager::needsResync = true;
    
    // Update the UI immediately so arcs 17-20 show the new LFO values right away
    updateParameterLabels();
}

void UiManager::setMenuState(MenuState state) {
    currentMenuState = state;
    
    // Default: Hide all secondary containers
    lv_obj_add_flag(ui_ContainerChannel, LV_OBJ_FLAG_HIDDEN);
    lv_obj_add_flag(ui_ContainerCCPage, LV_OBJ_FLAG_HIDDEN);
    lv_obj_add_flag(ui_ContainerSelectLayer, LV_OBJ_FLAG_HIDDEN);
    lv_obj_add_flag(ui_ContainerSelectTop, LV_OBJ_FLAG_HIDDEN);
    lv_obj_add_flag(ui_ContainerKeyboard, LV_OBJ_FLAG_HIDDEN);
    
    // Highlight main menu buttons
    lv_obj_t* menuBtns[] = { ui_ButtonGeneral1, ui_ButtonGeneral2, ui_ButtonGeneral3, ui_ButtonGeneral4 };
    for (int i = 0; i < 4; i++) {
        if (i == (int)state) lv_obj_set_style_bg_color(menuBtns[i], lv_color_hex(0x184873), LV_PART_MAIN | LV_STATE_DEFAULT);
        else lv_obj_set_style_bg_color(menuBtns[i], lv_color_hex(0x000000), LV_PART_MAIN | LV_STATE_DEFAULT);
    }
    
    switch (state) {
        case MENU_CHANNEL:
            lv_obj_clear_flag(ui_ContainerChannel, LV_OBJ_FLAG_HIDDEN);
            lv_obj_clear_flag(ui_ContainerCCPage, LV_OBJ_FLAG_HIDDEN);
            break;
        case MENU_TRACK:
            lv_obj_clear_flag(ui_ContainerSelectLayer, LV_OBJ_FLAG_HIDDEN);
            lv_obj_clear_flag(ui_ContainerSelectTop, LV_OBJ_FLAG_HIDDEN);
            updateTrackButtonLabels();
            break;
        case MENU_KEYBOARD:
            lv_obj_clear_flag(ui_ContainerKeyboard, LV_OBJ_FLAG_HIDDEN);
            break;
        case MENU_TEMPLATES:
            // Future implementation
            break;
    }
    
    updateLFOButtonColors(); // Refresh Settings 1/2 labels if needed
}

void UiManager::updateTrackButtonLabels() {
    lv_obj_t* btns[] = { ui_ButtonTop1, ui_ButtonTop2, ui_ButtonTop3, ui_ButtonTop4 };
    lv_obj_t* labels[] = { ui_LabelButtonTop1, ui_LabelButtonTop2, ui_LabelButtonTop3, ui_LabelButtonTop4 };
    lv_obj_t* layerBtns[] = { ui_ButtonLayer1, ui_ButtonLayer2, ui_ButtonLayer3, ui_ButtonLayer4 };
    lv_obj_t* layerLabels[] = { ui_LabelButtonLayer1, ui_LabelButtonLayer2, ui_LabelButtonLayer3, ui_LabelButtonLayer4 };

    for (int i = 0; i < 4; i++) {
        // Highlight active layer button with solid colors (no blueish transparency bleed)
        if (i == MidiManager::activeTrackLayer) {
            lv_obj_set_style_bg_color(layerBtns[i], lv_color_hex(0x184873), LV_PART_MAIN | LV_STATE_DEFAULT);
            lv_obj_set_style_bg_opa(layerBtns[i], 255, LV_PART_MAIN | LV_STATE_DEFAULT);
        } else {
            lv_obj_set_style_bg_color(layerBtns[i], lv_color_hex(0x000000), LV_PART_MAIN | LV_STATE_DEFAULT);
            lv_obj_set_style_bg_opa(layerBtns[i], 255, LV_PART_MAIN | LV_STATE_DEFAULT);
        }

        // Force top action buttons to solid black when not pressed
        lv_obj_set_style_bg_color(btns[i], lv_color_hex(0x000000), LV_PART_MAIN | LV_STATE_DEFAULT);
        lv_obj_set_style_bg_opa(btns[i], 255, LV_PART_MAIN | LV_STATE_DEFAULT);

        // Update layer labels
        if (layerLabels[i]) {
            lv_label_set_text(layerLabels[i], MidiManager::trackLayerLabels[i].c_str());
        }

        // Update top action button labels
        if (labels[i]) {
            lv_label_set_text(labels[i], MidiManager::trackActions[MidiManager::activeTrackLayer][i].label.c_str());
        }
    }
}

// ========== GLOBAL EVENT HANDLERS (Delegated from .ino) ==========

void ui_event_MainMenuGeneric(lv_event_t *e) {
    if (lv_event_get_code(e) == LV_EVENT_CLICKED) {
        UiManager::MenuState state = (UiManager::MenuState)(intptr_t)lv_obj_get_user_data(lv_event_get_target(e));
        UiManager::setMenuState(state);
    }
}

void ui_event_TrackLayerGeneric(lv_event_t *e) {
    lv_event_code_t code = lv_event_get_code(e);
    int layer = (int)(intptr_t)lv_obj_get_user_data(lv_event_get_target(e));
    if (code == LV_EVENT_PRESSED) {
        MidiManager::sendTrackLayerPress(layer);
    } else if (code == LV_EVENT_RELEASED) {
        MidiManager::sendTrackLayerRelease(layer);
    } else if (code == LV_EVENT_CLICKED) {
        MidiManager::activeTrackLayer = layer;
        UiManager::updateTrackButtonLabels();
    }
}

void ui_event_TrackTopGeneric(lv_event_t *e) {
    lv_event_code_t code = lv_event_get_code(e);
    int index = (int)(intptr_t)lv_obj_get_user_data(lv_event_get_target(e));
    if (code == LV_EVENT_PRESSED) {
        MidiManager::sendTrackActionPress(index);
        lv_obj_set_style_bg_color(lv_event_get_target(e), lv_color_hex(0x184873), LV_PART_MAIN | LV_STATE_DEFAULT);
        lv_obj_set_style_bg_opa(lv_event_get_target(e), 255, LV_PART_MAIN | LV_STATE_DEFAULT);
    } else if (code == LV_EVENT_RELEASED) {
        MidiManager::sendTrackActionRelease(index);
        lv_obj_set_style_bg_color(lv_event_get_target(e), lv_color_hex(0x000000), LV_PART_MAIN | LV_STATE_DEFAULT);
        lv_obj_set_style_bg_opa(lv_event_get_target(e), 255, LV_PART_MAIN | LV_STATE_DEFAULT);
    }
}

void ui_event_KeyboardGeneric(lv_event_t *e) {
    lv_event_code_t code = lv_event_get_code(e);
    int keyIndex = (int)(intptr_t)lv_obj_get_user_data(lv_event_get_target(e));
    uint8_t note = 60 + (MidiManager::keyboardOctave * 12) + keyIndex;
    
    if (code == LV_EVENT_PRESSED) {
        MidiManager::sendNoteOn(note, MidiManager::currentMidiChannel);
        lv_obj_set_style_bg_color(lv_event_get_target(e), lv_color_hex(0x184873), LV_PART_MAIN | LV_STATE_DEFAULT);
        lv_obj_set_style_bg_opa(lv_event_get_target(e), 255, LV_PART_MAIN | LV_STATE_DEFAULT);
    } else if (code == LV_EVENT_RELEASED) {
        MidiManager::sendNoteOff(note, MidiManager::currentMidiChannel);
        lv_obj_set_style_bg_color(lv_event_get_target(e), lv_color_hex(0x000000), LV_PART_MAIN | LV_STATE_DEFAULT);
        lv_obj_set_style_bg_opa(lv_event_get_target(e), 255, LV_PART_MAIN | LV_STATE_DEFAULT);
    }
}
