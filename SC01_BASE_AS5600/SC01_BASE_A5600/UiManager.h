#ifndef UI_MANAGER_H
#define UI_MANAGER_H

#include <lvgl.h>
#include "Config.h"
#include "MidiManager.h"
#include "LfoEngine.h"

// Forward declaration of LGFX class (implementation in .cpp)
class LGFX;

class UiManager {
public:
    static void init();
    static void update();
    
    // UI synchronization
    static void loadValuesForCurrentState();
    static void refreshDisplay();
    static void refreshSinglePot(int index);
    static void setActiveArc(int index);
    static void setModulationTarget(int index);
    
    // Color updates
    static void updateChannelButtonColors();
    static void updatePageButtonColors();
    static void updateLFOButtonColors();
    static void updateModulationUIColors();
    
    // Arcs and Labels
    static void updateParameterLabels();
    static void updateSelectedArcMod(float mixedLFOValue);
    static void updateMainLFODisplay();
    
    // Accessors for UI objects (needed for event handlers)
    static lv_obj_t* getArc(int index);
    static lv_obj_t* getLabelValue(int index);
    static lv_obj_t* getLabelName(int index);
    static void handleKeyboardEvent(lv_event_t *e);
    
    // Mode toggles
    static void toggleLfoMode();
    static void toggleMixerMode();
    static void setCcRow(int row);
    static void syncLfoArcValues(); // Syncs potentiometerValues[16-19] to current LFO state
    
    enum MenuState { MENU_CHANNEL, MENU_TRACK, MENU_KEYBOARD, MENU_TEMPLATES };
    static MenuState currentMenuState;
    static void setMenuState(MenuState state);
    static void updateTrackButtonLabels();

    static int activeArcIndex;
    static int activeEditPage;
    static int activeEditChannel;
    static bool isLfoMode;
    static bool isMixerMode;
    static bool isShiftActive;
    static bool shiftWasUsed;
    static bool previousMixerMode;
    static int ccRowIndex;
    static int currentMixerPage;

    static void setMixerPage(int page);

    // Global UI Object Arrays (made public for direct access by event handlers in .ino for now)
    static lv_obj_t *ui_ButtonChannel[16];
    static lv_obj_t *ui_ButtonPage[8];
    static lv_obj_t *ui_ButtonArc[NUM_ARCS];
    static lv_obj_t *ui_Arc[NUM_ARCS];
    static lv_obj_t *ui_LabelValue[NUM_ARCS];
    static lv_obj_t *ui_LabelName[NUM_ARCS];
    static lv_obj_t *ui_ArcMod[NUM_ARCS];
    static LGFX* tft;

private:
    static void initArcArrays();
    static void initButtons();
    static void setupMainLFO();
    static lv_disp_draw_buf_t draw_buf;
    static lv_color_t* buf;
    
    // LFO visualization objects
    static lv_obj_t* lfoCanvas;
    static lv_draw_line_dsc_t main_line_dsc;
};

#endif // UI_MANAGER_H
