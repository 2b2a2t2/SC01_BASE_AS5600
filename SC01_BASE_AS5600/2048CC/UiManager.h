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
    static void updateTrackButtonLabels();
    static void updateTemplateButtonLabels();
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

    // ---- Keyboard Submode ----
    enum KeyboardSubmode { SUBMODE_KEYS, SUBMODE_CHORD };
    static KeyboardSubmode keyboardSubmode;

    // Keys submode state (keyboardOctave lives in MidiManager)
    static int selectedScale;          // index into scaleNames / scalePatterns
    static int velocityCurve;          // 0=Linear, 1=Soft, 2=Hard, 3=Fixed
    static int modWheelValue;          // 0-127

    // Chord submode state (J-6 style chord set player)
    static int chordOctave;            // -2 to +2 (relative to middle C)
    static int selectedChordSet;       // 0-99

    static const int NUM_SCALES;
    static const char* scaleNames[19];
    static const int scalePatterns[19][12];
    static const int scaleLengths[19];

    static const char* noteNames[12];

    // J-6 Chord Set data (loaded from SD card /chord_sets.json)
    static String chordSetNames[100];
    static String chordSetNotes[100][12];
    static void loadChordSets();
    static const int NUM_CHORD_SETS = 100;

    static void updateKeyboardColors();

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

    static bool getPotDetent(int potIndex);
};

#endif // UI_MANAGER_H
