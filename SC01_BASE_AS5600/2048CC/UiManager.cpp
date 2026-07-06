#include "UiManager.h"
#define LGFX_USE_V1
#include <LovyanGFX.hpp>
#include "src/UI/ui.h"
#include "src/UI/components/ui_comp.h"
#include "src/UI/components/ui_comp_containerpots.h"
#include "HardwareHAL.h"
#include "SensorManager.h"
#include "StorageManager.h"
#include "WebServerManager.h"
#include <ArduinoJson.h>

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

// ---- Keyboard Submode State ----
UiManager::KeyboardSubmode UiManager::keyboardSubmode = UiManager::SUBMODE_KEYS;
int UiManager::selectedScale = 0;
int UiManager::velocityCurve = 0;
int UiManager::modWheelValue = 64;
int UiManager::chordOctave = 0;
int UiManager::selectedChordSet = 0;

// ---- Scale / Chord Data Tables ----
const int UiManager::NUM_SCALES = 19;
const char* UiManager::scaleNames[19] = {
    "Chromatic", "Major", "Nat.Minor", "Dorian",
    "Mixolydian", "Maj.Pent", "Min.Pent", "Blues",
    "Harm.Min", "Mel.Min", "WholeTone", "Dim(WH)",
    "Phryg.Dom", "Hung.Min", "DblHarm", "Maq.Hijaz",
    "Maq.Rast", "Freygish", "Hirajoshi"
};
const int UiManager::scalePatterns[19][12] = {
    {0,1,2,3,4,5,6,7,8,9,10,11}, // Chromatic (index 0)
    {0,2,4,5,7,9,11},            // Major
    {0,2,3,5,7,8,10},            // Natural Minor
    {0,2,3,5,7,9,10},            // Dorian
    {0,2,4,5,7,9,10},            // Mixolydian
    {0,2,4,7,9},                 // Major Pentatonic
    {0,3,5,7,10},                // Minor Pentatonic
    {0,3,5,6,7,10},              // Blues
    {0,2,3,5,7,8,11},            // Harmonic Minor
    {0,2,3,5,7,9,11},            // Melodic Minor
    {0,2,4,6,8,10},              // Whole Tone
    {0,2,3,5,6,8,9,11},          // Diminished (Whole-Half)
    {0,1,4,5,7,8,10},            // Phrygian Dominant
    {0,2,3,6,7,8,11},            // Hungarian Minor
    {0,1,4,5,7,8,11},            // Double Harmonic
    {0,1,4,5,7,8,10},            // Maqam Hijaz
    {0,2,4,5,7,9,10},            // Maqam Rast
    {0,1,4,5,7,8,10},            // Freygish
    {0,2,3,7,8}                  // Hirajoshi
};
const int UiManager::scaleLengths[19] = {
    12,7,7,7,7,5,5,6,7,7,6,8,7,7,7,7,7,7,5
};

const char* UiManager::noteNames[12] = {
    "C", "C#", "D", "D#", "E", "F", "F#", "G", "G#", "A", "A#", "B"
};

// ---- J-6 Chord Set Data (loaded from SD card /chord_sets.json) ----
// Only set *names* are kept for all 100 sets (needed by the UI selector).
// The 12 chord strings for the currently-active set are held in chordSetNotes[12].
String UiManager::chordSetNames[100];
String UiManager::chordSetNotes[12];

// loadChordSets() — loads only the 100 set names using a filtered parse so that
// no full-document allocation is needed. Then immediately loads the chords for
// the currently selected set.
void UiManager::loadChordSets() {
    if (!SD.exists("/chord_sets.json")) {
        Serial.println("ERROR: chord_sets.json not found on SD card!");
        for (int i = 0; i < 100; i++) chordSetNames[i] = "Empty";
        for (int j = 0; j < 12; j++) chordSetNotes[j] = "C";
        return;
    }
    File file = SD.open("/chord_sets.json", FILE_READ);
    if (!file) {
        Serial.println("ERROR: Could not open chord_sets.json for names");
        for (int i = 0; i < 100; i++) chordSetNames[i] = "Empty";
        for (int j = 0; j < 12; j++) chordSetNotes[j] = "C";
        return;
    }

    // Use a filter document so ArduinoJson only allocates memory for "name" fields.
    JsonDocument filterDoc;
    filterDoc["sets"][0]["name"] = true;

    JsonDocument doc;
    DeserializationError error = deserializeJson(doc, file,
        DeserializationOption::Filter(filterDoc));
    file.close();

    if (error) {
        Serial.print("ERROR: Failed to parse chord_sets.json (names): ");
        Serial.println(error.c_str());
        for (int i = 0; i < 100; i++) chordSetNames[i] = "Empty";
    } else {
        JsonArray sets = doc["sets"];
        int count = sets ? min((int)sets.size(), 100) : 0;
        for (int i = 0; i < count; i++)
            chordSetNames[i] = sets[i]["name"].as<String>();
        for (int i = count; i < 100; i++)
            chordSetNames[i] = "";
        Serial.printf("Loaded %d chord set names from /chord_sets.json\n", count);
    }

    // Load chords for the currently-selected set
    loadChordSetNotes(selectedChordSet);
}

// loadChordSetNotes() — loads the 12 chords for one set from the SD card.
// Uses a small filtered JsonDocument so only one set entry is allocated.
void UiManager::loadChordSetNotes(int setIndex) {
    // Clear first
    for (int j = 0; j < 12; j++) chordSetNotes[j] = "C";

    if (!SD.exists("/chord_sets.json")) {
        Serial.println("ERROR: chord_sets.json not found (loadChordSetNotes)");
        return;
    }
    File file = SD.open("/chord_sets.json", FILE_READ);
    if (!file) {
        Serial.println("ERROR: Could not open chord_sets.json (loadChordSetNotes)");
        return;
    }

    // Filter: only read the "chords" field (skip "name", "id", etc.)
    JsonDocument filterDoc;
    filterDoc["sets"][0]["chords"] = true;

    JsonDocument doc;
    DeserializationError error = deserializeJson(doc, file,
        DeserializationOption::Filter(filterDoc));
    file.close();

    if (error) {
        Serial.print("ERROR: Failed to parse chord_sets.json (chords): ");
        Serial.println(error.c_str());
        return;
    }

    JsonArray sets = doc["sets"];
    if (!sets || setIndex < 0 || setIndex >= (int)sets.size()) {
        Serial.printf("WARN: chord set index %d out of range\n", setIndex);
        return;
    }

    JsonArray chords = sets[setIndex]["chords"];
    if (!chords) {
        Serial.printf("WARN: no chords array for set %d\n", setIndex);
        return;
    }

    for (int j = 0; j < 12; j++) {
        if (j < (int)chords.size()) chordSetNotes[j] = chords[j].as<String>();
        else chordSetNotes[j] = "C";
    }
    Serial.printf("Loaded chords for set %d (%s)\n", setIndex, chordSetNames[setIndex].c_str());
}

// ---- Chord Name Parser (J-6 voicing engine) ----
// Parses a chord name and fills the notes array with MIDI note numbers
// relative to rootNote (MIDI). Returns number of notes (0 on error).

static int parseRootFromName(const char*& p) {
    const uint8_t rootTab[] = {0,0,2,3,5,5,7,8,8,10,10,11}; // C, C#, D, D#, E, F, F#, G, G#, A, A#, B
    if (!p || !*p) return -1;
    uint8_t c = p[0];
    if (c >= 'a' && c <= 'z') c -= 32; // toupper
    if (c < 'A' || c > 'G') return -1;
    // Map letter to root index
    int root;
    switch (c) {
        case 'C': root = 0; break;
        case 'D': root = 2; break;
        case 'E': root = 4; break;
        case 'F': root = 5; break;
        case 'G': root = 7; break;
        case 'A': root = 9; break;
        case 'B': root = 11; break;
        default: return -1;
    }
    int consumed = 1;
    if (p[1] == '#') { root = (root + 1) % 12; consumed = 2; }
    else if (p[1] == 'b') { root = (root + 11) % 12; consumed = 2; }
    p += consumed;
    while (*p == ' ') p++; // skip spaces
    return root;
}

// Try to find the interval pattern matching the quality string.
// Returns number of matched chars, or 0.
static int matchChordQuality(const char* q, int* intervals, int& numNotes, bool& omit3rd) {
    omit3rd = false;
    // Order matters: check longer patterns first to avoid false prefix matches
    struct { const char* pat; int iv[8]; int n; bool om3; } patterns[] = {
        {"13(no3)",   {0,5,7,10,14,17,21},7,1},
        {"13(no 3)",  {0,5,7,10,14,17,21},7,1},
        {"(no3)",     {0,4,7},3,1},
        {"(no 3)",    {0,4,7},3,1},
        {"Maj13",     {0,4,7,11,14,17,21},7,0},
        {"maj13",     {0,4,7,11,14,17,21},7,0},
        {"sus2",      {0,2,7},3,0},
        {"sus4",      {0,5,7},3,0},
        {"sus9/13",   {0,5,7,10,14,21},6,0},
        {"m9/11",     {0,3,7,10,14,17},6,0},
        {"sus4/b9",   {0,5,7,10,13},5,0},
        {"9/#11",     {0,4,7,10,14,18},6,0},
        {"M9/#11",    {0,4,7,11,14,18},6,0},
        {"7/b13",     {0,4,7,10,20},5,0},
        {"Mb5/#9",    {0,4,6,15},4,0},
        {"7#5#9",     {0,4,8,10,15},5,0},
        {"9sus",      {0,5,7,10,14},5,0},
        {"13b9",      {0,4,7,10,13,17,21},7,0},
        {"dimM7",     {0,3,6,11},4,0},
        {"m7b5",      {0,3,6,10},4,0},
        {"dim7",      {0,3,6,9},4,0},
        {"m7/11",     {0,3,7,10,17},5,0},
        {"m7/b13",    {0,3,7,10,20},5,0},
        {"M7add6",    {0,4,7,11,9},5,0},
        {"m7add13",   {0,3,7,10,21},5,0},
        {"aug7",      {0,4,8,10},4,0},
        {"7sus4",     {0,5,7,10},4,0},
        {"7sus",      {0,5,7,10},4,0},
        {"7#9",       {0,4,7,10,15},5,0},
        {"7b9",       {0,4,7,10,13},5,0},
        {"7#5",       {0,4,8,10},4,0},
        {"7alt",      {0,4,7,10,13,15},6,0},
        {"M7#5",      {0,4,8,11},4,0},
        {"M7b5",      {0,4,6,11},4,0},
        {"m7b13",     {0,3,7,10,20},5,0},
        {"add9/b13",  {0,4,7,14,20},5,0},
        {"sus2b5",    {0,2,6},3,0},
        {"M9(no3)",   {0,7,11,14},4,1},
        {"maj9",      {0,4,7,11,14},5,0},
        {"7sus2",     {0,2,7,10},4,0},
        {"11sus",     {0,5,7,10,14,17},6,0},
        {"add9",      {0,4,7,14},4,0},
        {"add11",     {0,4,7,17},4,0},
        {"add2",      {0,2,4,7},4,0},
        {"add13",     {0,4,7,21},4,0},
        {"madd9",     {0,3,7,14},4,0},
        {"mb13",      {0,3,7,20},4,0},
        {"m7b13",     {0,3,7,10,20},5,0},
        {"m11",       {0,3,7,10,14,17},6,0},
        {"maj7",      {0,4,7,11},4,0},
        {"Maj7",      {0,4,7,11},4,0},
        {"Maj13",     {0,4,7,11,14,17,21},7,0},
        {"6/9",       {0,4,7,9,14},5,0},
        {"m7",        {0,3,7,10},4,0},
        {"M7",        {0,4,7,11},4,0},
        {"m9",        {0,3,7,10,14},5,0},
        {"M9",        {0,4,7,11,14},5,0},
        {"m13",       {0,3,7,10,14,17,21},7,0},
        {"M13",       {0,4,7,11,14,17,21},7,0},
        {"m6",        {0,3,7,9},4,0},
        {"mb5",       {0,3,6},3,0},
        {"#11",       {0,4,7,18},4,0},
        {"b13",       {0,4,7,20},4,0},
        {"dim",       {0,3,6},3,0},
        {"aug",       {0,4,8},3,0},
        {"sus",       {0,5,7},3,0},
        {"maj",       {0,4,7},3,0},
        {"m",         {0,3,7},3,0},
        {"M",         {0,4,7},3,0},
        {"7",         {0,4,7,10},4,0},
        {"9",         {0,4,7,10,14},5,0},
        {"11",        {0,4,7,10,14,17},6,0},
        {"13",        {0,4,7,10,14,17,21},7,0},
        {"6",         {0,4,7,9},4,0},
        {"5",         {0,7},2,0},
        {"4",         {0,5},2,0},
        {"",          {0,4,7},3,0}, // default major
    };
    for (auto& pat : patterns) {
        int pi = 0;
        while (pat.pat[pi] && q[pi] && pat.pat[pi] == q[pi]) pi++;
        if (pat.pat[pi] == '\0' || (pat.pat[pi] == ' ' && q[pi] == ' ')) {
            // matched
            for (int i = 0; i < pat.n; i++) intervals[i] = pat.iv[i];
            numNotes = pat.n;
            omit3rd = pat.om3;
            return pi;
        }
    }
    return 0;
}

// Strip trailing parenthesized content like (13), (no3), /#11 etc.
// Returns the stripped length (characters to skip at end)
static int stripParenthetical(const char* s) {
    int len = strlen(s);
    while (len > 0 && s[len-1] == ' ') len--; // strip trailing spaces
    // Check for trailing ) - find matching (
    if (len > 0 && s[len-1] == ')') {
        int depth = 0;
        for (int i = len - 1; i >= 0; i--) {
            if (s[i] == ')') depth++;
            else if (s[i] == '(') { depth--; if (depth == 0) return len - i; }
        }
    }
    return 0;
}

static int chordNameToNotes(const char* name, int rootNote, uint8_t* notes, int maxNotes) {
    if (!name || !*name || maxNotes < 1) return 0;
    
    const char* p = name;
    int root = parseRootFromName(p);
    if (root < 0) return 0;
    
    // Copy everything after root into quality buffer
    char qualBuf[32];
    int qi = 0;
    for (const char* s = p; *s && qi < 31; s++) {
        qualBuf[qi++] = *s;
    }
    qualBuf[qi] = '\0';
    
    // Handle slash: /X where X is a note letter → bass note
    // Otherwise (/#11, /b5, etc.) keep in qualBuf for pattern matching
    int bassRoot = -1;
    char* slash = strchr(qualBuf, '/');
    if (slash) {
        const char* sp = slash + 1;
        while (*sp == ' ') sp++;
        if (*sp >= 'A' && *sp <= 'G') {
            bassRoot = parseRootFromName(sp);
            *slash = '\0';
        }
    }
    
    int baseIntervals[8];
    int numNotes = 0;
    bool omit3rd = false;
    int matched = matchChordQuality(qualBuf, baseIntervals, numNotes, omit3rd);
    
    if (matched == 0) {
        baseIntervals[0] = 0; baseIntervals[1] = 4; baseIntervals[2] = 7;
        numNotes = 3;
    }
    
    // Handle parenthetical extensions like (13), (11), (9)
    int parenSkip = stripParenthetical(qualBuf);
    if (parenSkip > 0) {
        char inner[16];
        int innerLen = strlen(qualBuf) - parenSkip;
        if (innerLen > 0 && qualBuf[innerLen] == '(') {
            int ii = 0;
            for (int k = innerLen + 1; k < (int)strlen(qualBuf) - 1 && ii < 15; k++) {
                if (qualBuf[k] != ' ') inner[ii++] = qualBuf[k];
            }
            inner[ii] = '\0';
            if (strcmp(inner, "13") == 0) {
                baseIntervals[numNotes++] = 21;
            } else if (strcmp(inner, "11") == 0) {
                baseIntervals[numNotes++] = 17;
            } else if (strcmp(inner, "9") == 0) {
                baseIntervals[numNotes++] = 14;
            }
        }
    }
    
    // Handle residual content after pattern match
    if (matched > 0) {
        const char* residual = qualBuf + matched;
        while (*residual == ' ') residual++;
        if (strncmp(residual, "add", 3) == 0) {
            int ext = atoi(residual + 3);
            if (ext == 13) baseIntervals[numNotes++] = 21;
            else if (ext == 11) baseIntervals[numNotes++] = 17;
            else if (ext == 9) baseIntervals[numNotes++] = 14;
        } else if (*residual == '/') {
            // Handle /alteration like /b5, /#5, /b13, /#11, /b9, /#9
            const char* a = residual + 1;
            if (a[0] == 'b') {
                if (strcmp(a + 1, "5") == 0) { /* b5 = dim5 = interval 6 */
                    for (int i = 0; i < numNotes; i++) {
                        if (baseIntervals[i] == 7) { baseIntervals[i] = 6; break; }
                    }
                } else if (strcmp(a + 1, "13") == 0) baseIntervals[numNotes++] = 20;
                else if (strcmp(a + 1, "9") == 0) baseIntervals[numNotes++] = 13;
            } else if (a[0] == '#') {
                if (strcmp(a + 1, "5") == 0) { /* #5 = aug5 = interval 8 */
                    for (int i = 0; i < numNotes; i++) {
                        if (baseIntervals[i] == 7) { baseIntervals[i] = 8; break; }
                    }
                } else if (strcmp(a + 1, "11") == 0) baseIntervals[numNotes++] = 18;
                else if (strcmp(a + 1, "9") == 0) baseIntervals[numNotes++] = 15;
            }
        }
    }
    
    if (numNotes <= 0) return 0;
    
    int count = 0;
    for (int i = 0; i < numNotes && count < maxNotes; i++) {
        int note = rootNote + root + baseIntervals[i];
        if (note < 0) note = 0;
        if (note > 127) note = 127;
        notes[count++] = (uint8_t)note;
    }
    
    if (bassRoot >= 0) {
        int bassNote = rootNote + bassRoot;
        if (bassNote < 0) bassNote = 0;
        if (bassNote > 127) bassNote = 127;
        if (bassNote != notes[0]) {
            while (bassNote > notes[0]) bassNote -= 12;
            if (bassNote < 0) bassNote = 0;
            for (int i = count; i > 0; i--) notes[i] = notes[i-1];
            notes[0] = (uint8_t)bassNote;
            count++;
        }
    }
    
    return count;
}

// Forward declarations for LVGL event handlers (defined at end of file)
static void lfo1EventHandler(lv_event_t *e);
static void lfo2EventHandler(lv_event_t *e);
static void lfo3EventHandler(lv_event_t *e);
static void lfo4EventHandler(lv_event_t *e);
static void lfoMixEventHandler(lv_event_t *e);
static void ui_event_ButtonArcGeneric(lv_event_t *e);
static void ui_event_ButtonArcModulationSelect(lv_event_t *e);
static void ui_event_ArcGeneric(lv_event_t *e);
static void ui_event_ButtonChannelGeneric(lv_event_t *e);
static void ui_event_ButtonPageGeneric(lv_event_t *e);
static void ui_event_ButtonWifi(lv_event_t *e);
static void ui_event_MainMenuGeneric(lv_event_t *e);
static void ui_event_TrackLayerGeneric(lv_event_t *e);
static void ui_event_TrackTopGeneric(lv_event_t *e);
static void ui_event_KeyboardGeneric(lv_event_t *e);

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
    tft->setBrightness(SCREEN_BRIGHTNESS);
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
        lv_obj_add_event_cb(ui_ButtonArc[i], ui_event_ButtonArcGeneric, LV_EVENT_ALL, NULL);
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

    if (ui_WifiRoller) {
        String options = "";
        for (int i = 0; i < numWifiNetworks; i++) {
            if (i > 0) options += "\n";
            options += wifiNetworks[i].ssid;
        }
        lv_roller_set_options(ui_WifiRoller, options.c_str(), LV_ROLLER_MODE_INFINITE);

        // Pre-select the currently active SSID
        for (int i = 0; i < numWifiNetworks; i++) {
            if (strcmp(ssid, wifiNetworks[i].ssid) == 0) {
                lv_roller_set_selected(ui_WifiRoller, i, LV_ANIM_OFF);
                break;
            }
        }

        // Add event callback to handle value change
        lv_obj_add_event_cb(ui_WifiRoller, [](lv_event_t *e) {
            if (lv_event_get_code(e) == LV_EVENT_VALUE_CHANGED) {
                int selectedIndex = lv_roller_get_selected(ui_WifiRoller);
                if (selectedIndex >= 0 && selectedIndex < numWifiNetworks) {
                    ssid = wifiNetworks[selectedIndex].ssid;
                    password = wifiNetworks[selectedIndex].password;
                    Serial.print("Selected WiFi Network: ");
                    Serial.println(ssid);
                    
                    // If WiFi is currently enabled and connected, reconnect to the new network
                    if (WebServerManager::isWiFiEnabled()) {
                        WebServerManager::stopWiFi();
                        WebServerManager::startWiFi();
                    }
                }
            }
        }, LV_EVENT_VALUE_CHANGED, NULL);
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
                for (int i = 0; i < 4; i++) {
                    LfoEngine::savedMixAmounts[i] = LfoEngine::mixAmounts[i];
                    LfoEngine::mixAmounts[i] = 0;
                    if (isLfoMode && LfoEngine::mixMode) {
                        SensorManager::potentiometerValues[16 + i] = 0;
                        MidiManager::needsResync = true;
                    }
                }
                LfoEngine::refreshAnyMixActive();
            } else {
                for (int i = 0; i < 4; i++) {
                    LfoEngine::mixAmounts[i] = LfoEngine::savedMixAmounts[i];
                    if (isLfoMode && LfoEngine::mixMode) {
                        SensorManager::potentiometerValues[16 + i] = LfoEngine::mixAmounts[i] * 127.0f;
                        MidiManager::needsResync = true;
                    }
                }
                LfoEngine::refreshAnyMixActive();
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

    loadChordSets();
    setMenuState(MENU_CHANNEL);
}

void UiManager::update() {
    static unsigned long lastLvglTick = 0;
    unsigned long now = millis();
    if (now - lastLvglTick >= 5) {
        lastLvglTick = now;
        lv_timer_handler();
    }
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

bool UiManager::getPotDetent(int potIndex) {
    if (potIndex < 16) {
        if (isMixerMode) {
            int cc = MidiManager::mixerPageCCs[currentMixerPage][potIndex];
            int ch = MidiManager::mixerPageChannels[currentMixerPage][potIndex];
            int p = (cc - 1) / 16;
            int pot = (cc - 1) % 16;
            if (p >= 0 && p < NUM_PAGES && ch >= 0 && ch < NUM_CHANNELS) {
                return MidiManager::storedPotentiometerDetents[p][ch][pot];
            }
            return false;
        }
        return MidiManager::storedPotentiometerDetents[MidiManager::currentPage][MidiManager::currentMidiChannel][potIndex];
    }
    if (!isLfoMode) {
        int rowSourcePot = ccRowIndex * 4 + (potIndex - 16);
        return MidiManager::storedPotentiometerDetents[0][MidiManager::currentMidiChannel][rowSourcePot];
    }
    return false;
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
            lv_arc_set_mode(arc, getPotDetent(i) ? LV_ARC_MODE_SYMMETRICAL : LV_ARC_MODE_NORMAL);
        }
        if (labelVal) {
            if (getPotDetent(i)) {
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
    
    if (potIndex >= 16 && currentMenuState == MENU_KEYBOARD) {
        // Handled entirely by updateParameterLabels
        return;
    }

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
            lv_arc_set_mode(arc, getPotDetent(potIndex) ? LV_ARC_MODE_SYMMETRICAL : LV_ARC_MODE_NORMAL);
        }
        if (labelVal) {
            if (getPotDetent(potIndex)) {
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
        if (ui_LabelButtonLFOMix) {
            if (keyboardSubmode == SUBMODE_KEYS) {
                lv_label_set_text(ui_LabelButtonLFOMix, "Chord");
            } else {
                lv_label_set_text(ui_LabelButtonLFOMix, "Keys");
            }
        }
        if (ui_LabelButtonSettings1) lv_label_set_text(ui_LabelButtonSettings1, "Oct -");
        if (ui_LabelButtonSettings2) lv_label_set_text(ui_LabelButtonSettings2, "Oct +");
        if (ui_LabelButtonSettings3) lv_label_set_text(ui_LabelButtonSettings3, "Oct Reset");
        if (ui_LabelButtonSettings4) {
            lv_label_set_text(ui_LabelButtonSettings4, keyboardSubmode == SUBMODE_KEYS ? "Chrd>>" : "Keys>>");
        }
        
        // Highlight LFOMix button to indicate active submode
        lv_color_t submodeColor = keyboardSubmode == SUBMODE_CHORD ? lv_color_hex(0xAA5500) : lv_color_hex(0x00AA55);
        lv_obj_set_style_bg_color(ui_ButtonLFOMix, submodeColor, LV_PART_MAIN | LV_STATE_DEFAULT);
        lv_obj_set_style_bg_opa(ui_ButtonLFOMix, 255, LV_PART_MAIN | LV_STATE_DEFAULT);
        lv_obj_set_style_text_color(ui_LabelButtonLFOMix, lv_color_hex(0xFFFFFF), LV_PART_MAIN | LV_STATE_DEFAULT);

        // Indicate current octave (highlight if not zero)
        int oct = (keyboardSubmode == SUBMODE_CHORD) ? chordOctave : MidiManager::keyboardOctave;
        if (oct != 0) {
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
    // -- Keyboard Submode Display --
    if (currentMenuState == MENU_KEYBOARD) {
        if (keyboardSubmode == SUBMODE_KEYS) {
            int dispOct = MidiManager::keyboardOctave;
            if (ui_LabelValue17) lv_label_set_text_fmt(ui_LabelValue17, "%+d", dispOct);
            if (ui_LabelValue18) lv_label_set_text(ui_LabelValue18, scaleNames[selectedScale]);
            const char* curves[] = { "Lin", "Soft", "Hard", "Fixd" };
            if (ui_LabelValue19) lv_label_set_text(ui_LabelValue19, curves[velocityCurve]);
            if (ui_LabelValue20) lv_label_set_text_fmt(ui_LabelValue20, "%d", modWheelValue);

            if (ui_LabelPot17) lv_label_set_text(ui_LabelPot17, "Octave");
            if (ui_LabelPot18) lv_label_set_text(ui_LabelPot18, "Scale");
            if (ui_LabelPot19) lv_label_set_text(ui_LabelPot19, "Vel");
            if (ui_LabelPot20) lv_label_set_text(ui_LabelPot20, "Mod");

            if (ui_Arc17) lv_arc_set_value(ui_Arc17, constrain(map(MidiManager::keyboardOctave + 4, 0, 8, 0, 127), 0, 127));
            if (ui_Arc18) lv_arc_set_value(ui_Arc18, constrain(map(selectedScale, 0, NUM_SCALES - 1, 0, 127), 0, 127));
            if (ui_Arc19) lv_arc_set_value(ui_Arc19, constrain(map(velocityCurve, 0, 3, 0, 127), 0, 127));
            if (ui_Arc20) lv_arc_set_value(ui_Arc20, constrain(modWheelValue, 0, 127));

            if (ui_Arc20) {
                lv_arc_set_mode(ui_Arc20, LV_ARC_MODE_NORMAL);
                lv_obj_set_style_arc_opa(ui_Arc20, 255, LV_PART_INDICATOR | LV_STATE_DEFAULT);
            }
        } else if (keyboardSubmode == SUBMODE_CHORD) {
            int setNum = selectedChordSet + 1;
            if (ui_LabelValue17) lv_label_set_text_fmt(ui_LabelValue17, "%+d", chordOctave);
            if (ui_LabelValue18) lv_label_set_text_fmt(ui_LabelValue18, "Set %d", setNum);
            if (ui_LabelValue19) lv_label_set_text(ui_LabelValue19, "");
            if (ui_LabelValue20) lv_label_set_text(ui_LabelValue20, "");

            if (ui_LabelPot17) lv_label_set_text(ui_LabelPot17, "Octave");
            // LabelPot18 shows the chord set name (acts as a title/label for the selected set)
            if (ui_LabelPot18) lv_label_set_text(ui_LabelPot18, chordSetNames[selectedChordSet].c_str());
            if (ui_LabelPot19) lv_label_set_text(ui_LabelPot19, "");
            if (ui_LabelPot20) lv_label_set_text(ui_LabelPot20, "");

            if (ui_Arc17) lv_arc_set_value(ui_Arc17, constrain(map(chordOctave + 2, 0, 4, 0, 127), 0, 127));
            if (ui_Arc18) lv_arc_set_value(ui_Arc18, constrain(map(selectedChordSet, 0, NUM_CHORD_SETS - 1, 0, 127), 0, 127));
            if (ui_Arc19) lv_arc_set_value(ui_Arc19, 0);
            if (ui_Arc20) lv_arc_set_value(ui_Arc20, 0);

            if (ui_Arc20) {
                lv_arc_set_mode(ui_Arc20, LV_ARC_MODE_NORMAL);
                lv_obj_set_style_arc_opa(ui_Arc20, 255, LV_PART_INDICATOR | LV_STATE_DEFAULT);
            }
        }
        // Keyboard mode: arcs 1-16 are behind the keyboard panel, skip them entirely
        return;
    }

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
updateArcs1to16:
    int baseCC = MidiManager::currentPage * 16;
    for (int i = 0; i < 16; i++) {
        int val = (int)SensorManager::potentiometerValues[i];
        lv_obj_t* arc = getArc(i);
        lv_obj_t* labelVal = getLabelValue(i);
        lv_obj_t* labelName = getLabelName(i);
        if (arc) {
            lv_arc_set_value(arc, val);
            lv_arc_set_mode(arc, getPotDetent(i) ? LV_ARC_MODE_SYMMETRICAL : LV_ARC_MODE_NORMAL);
        }
        if (labelVal) {
            if (getPotDetent(i)) {
                String s = (val - 64 >= 0 ? "+" : "") + String(val - 64);
                lv_label_set_text(labelVal, s.c_str());
            } else {
                lv_label_set_text_fmt(labelVal, "%d", val);
            }
        }
        if (labelName) {
            if (isMixerMode) {
                lv_label_set_text(labelName, MidiManager::getMixerLabel(currentMixerPage, i).c_str());
            } else {
                String label = MidiManager::currentArcLabels[i];
                if (label.length() > 0 && !label.startsWith("CC ")) lv_label_set_text(labelName, label.c_str());
                else lv_label_set_text_fmt(labelName, "CC %d", baseCC + i + 1);
            }
        }
    }

    // Update bottom arcs if NOT in LFO mode and NOT in keyboard mode
    if (!isLfoMode && currentMenuState != MENU_KEYBOARD) {
        int startCC = ccRowIndex * 4;
        for (int i = 0; i < 4; i++) {
            int potIdx = 16 + i;
            int ccVal = (int)SensorManager::potentiometerValues[potIdx];
            lv_obj_t* arc = getArc(potIdx);
            lv_obj_t* labelVal = getLabelValue(potIdx);
            lv_obj_t* labelName = getLabelName(potIdx);
            if (arc) {
                lv_arc_set_value(arc, ccVal);
                lv_arc_set_mode(arc, getPotDetent(potIdx) ? LV_ARC_MODE_SYMMETRICAL : LV_ARC_MODE_NORMAL);
            }
            if (labelVal) {
                if (getPotDetent(potIdx)) {
                    String s = (ccVal - 64 >= 0 ? "+" : "") + String(ccVal - 64);
                    lv_label_set_text(labelVal, s.c_str());
                } else {
                    lv_label_set_text_fmt(labelVal, "%d", ccVal);
                }
            }
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

void UiManager::updateKeyboardColors() {
    lv_obj_t* kbButtons[] = { ui_ButtonKeyboard1, ui_ButtonKeyboard2, ui_ButtonKeyboard3, ui_ButtonKeyboard4,
                             ui_ButtonKeyboard5, ui_ButtonKeyboard6, ui_ButtonKeyboard7, ui_ButtonKeyboard8,
                             ui_ButtonKeyboard9, ui_ButtonKeyboard10, ui_ButtonKeyboard11, ui_ButtonKeyboard12 };
    lv_obj_t* kbLabels[] = { ui_LabelButtonKeyboard1, ui_LabelButtonKeyboard2, ui_LabelButtonKeyboard3,
                             ui_LabelButtonKeyboard4, ui_LabelButtonKeyboard5, ui_LabelButtonKeyboard6,
                             ui_LabelButtonKeyboard7, ui_LabelButtonKeyboard8, ui_LabelButtonKeyboard9,
                             ui_LabelButtonKeyboard10, ui_LabelButtonKeyboard11, ui_LabelButtonKeyboard12 };

    for (int i = 0; i < 12; i++) {
        lv_obj_set_style_bg_color(kbButtons[i], lv_color_hex(0x000000), LV_PART_MAIN | LV_STATE_DEFAULT);
        lv_obj_set_style_bg_opa(kbButtons[i], 255, LV_PART_MAIN | LV_STATE_DEFAULT);
    }

    if (keyboardSubmode == SUBMODE_CHORD) {
        // Chord submode: clear borders, label keys with chord names
        for (int i = 0; i < 12; i++) {
            lv_obj_set_style_border_width(kbButtons[i], 0, LV_PART_MAIN | LV_STATE_DEFAULT);
            // chordSetNotes[12] always holds the active set's chords
            if (kbLabels[i]) lv_label_set_text(kbLabels[i], chordSetNotes[i].c_str());
        }
    } else {
        // Keys submode: scale outline, labels show note names
        const int* pattern = scalePatterns[selectedScale];
        int len = scaleLengths[selectedScale];
        for (int i = 0; i < 12; i++) {
            bool inScale = false;
            for (int s = 0; s < len; s++) {
                if (pattern[s] == i) { inScale = true; break; }
            }
            if (inScale) {
                lv_obj_set_style_border_color(kbButtons[i], lv_color_hex(0x00FF00), LV_PART_MAIN | LV_STATE_DEFAULT);
                lv_obj_set_style_border_width(kbButtons[i], 2, LV_PART_MAIN | LV_STATE_DEFAULT);
            } else {
                lv_obj_set_style_border_color(kbButtons[i], lv_color_hex(0xFF0000), LV_PART_MAIN | LV_STATE_DEFAULT);
                lv_obj_set_style_border_width(kbButtons[i], 2, LV_PART_MAIN | LV_STATE_DEFAULT);
            }
            if (kbLabels[i]) lv_label_set_text(kbLabels[i], noteNames[i]);
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
                    float baseVal = MidiManager::mixerPageValues[0][i];
                    int lfoValue = constrain((int)(baseVal + (mixedLFOValue - 0.5f) * 127.0f), 0, 127);
                    lv_arc_set_value(ui_ArcMod[i], lfoValue);
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
        int targetPage = MidiManager::modulationTargetPage;
        int targetChannel = MidiManager::modulationTargetChannel;
        int baseValue = MidiManager::storedMidiCCValues[targetPage][targetChannel][arcIndex];
        int lfoValue = constrain((int)(baseValue + (mixedLFOValue - 0.5f) * 127.0f), 0, 127);
        lv_arc_set_value(ui_ArcMod[arcIndex], lfoValue);
    }
}

void UiManager::updateMainLFODisplay() {
    if (!lfoCanvas) return;
    if (!LfoEngine::mainBufferDirty) return;
    LfoEngine::mainBufferDirty = false;

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
    if (currentMenuState == MENU_KEYBOARD) return;
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
            keyboardSubmode = SUBMODE_KEYS;
            lv_obj_clear_flag(ui_ContainerKeyboard, LV_OBJ_FLAG_HIDDEN);
            updateKeyboardColors();
            updateParameterLabels();
            break;
        case MENU_TEMPLATES:
            lv_obj_clear_flag(ui_ContainerSelectLayer, LV_OBJ_FLAG_HIDDEN);
            lv_obj_clear_flag(ui_ContainerSelectTop, LV_OBJ_FLAG_HIDDEN);
            updateTemplateButtonLabels();
            break;
    }
    
    updateLFOButtonColors(); // Refresh Settings 1/2 labels if needed
    updateParameterLabels(); // Refresh arc labels/values
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

void UiManager::updateTemplateButtonLabels() {
    lv_obj_t* btns[] = { ui_ButtonTop1, ui_ButtonTop2, ui_ButtonTop3, ui_ButtonTop4 };
    lv_obj_t* labels[] = { ui_LabelButtonTop1, ui_LabelButtonTop2, ui_LabelButtonTop3, ui_LabelButtonTop4 };
    lv_obj_t* layerBtns[] = { ui_ButtonLayer1, ui_ButtonLayer2, ui_ButtonLayer3, ui_ButtonLayer4 };
    lv_obj_t* layerLabels[] = { ui_LabelButtonLayer1, ui_LabelButtonLayer2, ui_LabelButtonLayer3, ui_LabelButtonLayer4 };

    for (int i = 0; i < 4; i++) {
        // Reset colors
        lv_obj_set_style_bg_color(btns[i], lv_color_hex(0x000000), LV_PART_MAIN | LV_STATE_DEFAULT);
        lv_obj_set_style_bg_opa(btns[i], 255, LV_PART_MAIN | LV_STATE_DEFAULT);
        lv_obj_set_style_bg_color(layerBtns[i], lv_color_hex(0x000000), LV_PART_MAIN | LV_STATE_DEFAULT);
        lv_obj_set_style_bg_opa(layerBtns[i], 255, LV_PART_MAIN | LV_STATE_DEFAULT);

        if (labels[i]) {
            lv_label_set_text(labels[i], MidiManager::templateLabels[i].c_str());
        }
        if (layerLabels[i]) {
            lv_label_set_text(layerLabels[i], MidiManager::templateLabels[i + 4].c_str());
        }
    }
}


// ========== PRIMARY LVGL EVENT HANDLERS ==========

static void ui_event_MainMenuGeneric(lv_event_t *e) {
    if (lv_event_get_code(e) == LV_EVENT_CLICKED) {
        UiManager::MenuState state = (UiManager::MenuState)(intptr_t)lv_obj_get_user_data(lv_event_get_target(e));
        UiManager::setMenuState(state);
    }
}

static void ui_event_TrackLayerGeneric(lv_event_t *e) {
    lv_event_code_t code = lv_event_get_code(e);
    int layer = (int)(intptr_t)lv_obj_get_user_data(lv_event_get_target(e));
    if (UiManager::currentMenuState == UiManager::MENU_TEMPLATES) {
        if (code == LV_EVENT_PRESSED) {
            MidiManager::sendTemplateActionPress(layer + 4);
            lv_obj_set_style_bg_color(lv_event_get_target(e), lv_color_hex(0x184873), LV_PART_MAIN | LV_STATE_DEFAULT);
        } else if (code == LV_EVENT_RELEASED) {
            MidiManager::sendTemplateActionRelease(layer + 4);
            lv_obj_set_style_bg_color(lv_event_get_target(e), lv_color_hex(0x000000), LV_PART_MAIN | LV_STATE_DEFAULT);
        }
    } else {
        if (code == LV_EVENT_PRESSED) {
            MidiManager::sendTrackLayerPress(layer);
        } else if (code == LV_EVENT_RELEASED) {
            MidiManager::sendTrackLayerRelease(layer);
        } else if (code == LV_EVENT_CLICKED) {
            MidiManager::activeTrackLayer = layer;
            UiManager::updateTrackButtonLabels();
        }
    }
}

static void ui_event_TrackTopGeneric(lv_event_t *e) {
    lv_event_code_t code = lv_event_get_code(e);
    int index = (int)(intptr_t)lv_obj_get_user_data(lv_event_get_target(e));
    if (code == LV_EVENT_PRESSED) {
        if (UiManager::currentMenuState == UiManager::MENU_TEMPLATES) {
            MidiManager::sendTemplateActionPress(index);
        } else {
            MidiManager::sendTrackActionPress(index);
        }
        lv_obj_set_style_bg_color(lv_event_get_target(e), lv_color_hex(0x184873), LV_PART_MAIN | LV_STATE_DEFAULT);
        lv_obj_set_style_bg_opa(lv_event_get_target(e), 255, LV_PART_MAIN | LV_STATE_DEFAULT);
    } else if (code == LV_EVENT_RELEASED) {
        if (UiManager::currentMenuState == UiManager::MENU_TEMPLATES) {
            MidiManager::sendTemplateActionRelease(index);
        } else {
            MidiManager::sendTrackActionRelease(index);
        }
        lv_obj_set_style_bg_color(lv_event_get_target(e), lv_color_hex(0x000000), LV_PART_MAIN | LV_STATE_DEFAULT);
        lv_obj_set_style_bg_opa(lv_event_get_target(e), 255, LV_PART_MAIN | LV_STATE_DEFAULT);
    }
}

static bool isNoteInScale(int keyIndex, int scaleIdx) {
    const int* pattern = UiManager::scalePatterns[scaleIdx];
    int len = UiManager::scaleLengths[scaleIdx];
    for (int i = 0; i < len; i++) {
        if (pattern[i] == keyIndex) return true;
    }
    return false;
}

static int snapToScale(int keyIndex, int scaleIdx, int dir) {
    const int* pattern = UiManager::scalePatterns[scaleIdx];
    int len = UiManager::scaleLengths[scaleIdx];
    for (int offset = 0; offset < 12; offset++) {
        int test = (keyIndex + dir * offset + 12) % 12;
        for (int i = 0; i < len; i++) {
            if (pattern[i] == test) return test;
        }
    }
    return keyIndex;
}

static void ui_event_KeyboardGeneric(lv_event_t *e) {
    lv_event_code_t code = lv_event_get_code(e);
    lv_obj_t* target = lv_event_get_target(e);
    int keyIndex = (int)(intptr_t)lv_obj_get_user_data(target);

    if (UiManager::keyboardSubmode == UiManager::SUBMODE_CHORD) {
        // J-6 Chord Set mode: each key triggers a chord from the selected set
        uint8_t chordNotes[8];
        // chordSetNotes[12] always holds the active set's chords
        const char* chordName = UiManager::chordSetNotes[keyIndex].c_str();
        int baseNote = 60 + (UiManager::chordOctave * 12);
        int numNotes = chordNameToNotes(chordName, baseNote, chordNotes, 8);

        if (code == LV_EVENT_PRESSED) {
            for (int i = 0; i < numNotes; i++) {
                MidiManager::sendNoteOn(chordNotes[i], MidiManager::currentMidiChannel, UiManager::modWheelValue);
            }
            lv_obj_set_style_bg_color(target, lv_color_hex(0x184873), LV_PART_MAIN | LV_STATE_DEFAULT);
            lv_obj_set_style_bg_opa(target, 255, LV_PART_MAIN | LV_STATE_DEFAULT);
        } else if (code == LV_EVENT_RELEASED) {
            for (int i = 0; i < numNotes; i++) {
                MidiManager::sendNoteOff(chordNotes[i], MidiManager::currentMidiChannel);
            }
            lv_obj_set_style_bg_color(target, lv_color_hex(0x000000), LV_PART_MAIN | LV_STATE_DEFAULT);
            lv_obj_set_style_bg_opa(target, 255, LV_PART_MAIN | LV_STATE_DEFAULT);
        }
        return;
    }

    // Keys submode: scale-sensitive single notes
    uint8_t note = 60 + (MidiManager::keyboardOctave * 12) + keyIndex;
    uint8_t velocity = UiManager::modWheelValue;

    if (code == LV_EVENT_PRESSED) {
        if (!isNoteInScale(keyIndex, UiManager::selectedScale)) {
            int snapped = snapToScale(keyIndex, UiManager::selectedScale, 1);
            note = 60 + (MidiManager::keyboardOctave * 12) + snapped;
        }
        MidiManager::sendNoteOn(note, MidiManager::currentMidiChannel, velocity);
        lv_obj_set_style_bg_color(target, lv_color_hex(0x184873), LV_PART_MAIN | LV_STATE_DEFAULT);
        lv_obj_set_style_bg_opa(target, 255, LV_PART_MAIN | LV_STATE_DEFAULT);
    } else if (code == LV_EVENT_RELEASED) {
        if (!isNoteInScale(keyIndex, UiManager::selectedScale)) {
            int snapped = snapToScale(keyIndex, UiManager::selectedScale, 1);
            note = 60 + (MidiManager::keyboardOctave * 12) + snapped;
        }
        MidiManager::sendNoteOff(note, MidiManager::currentMidiChannel);
        lv_obj_set_style_bg_color(target, lv_color_hex(0x000000), LV_PART_MAIN | LV_STATE_DEFAULT);
        lv_obj_set_style_bg_opa(target, 255, LV_PART_MAIN | LV_STATE_DEFAULT);
    }
}

static void ui_event_ButtonChannelGeneric(lv_event_t *e) {
    lv_event_code_t code = lv_event_get_code(e);
    lv_obj_t* target = lv_event_get_target(e);
    int channelIndex = (int)(intptr_t)lv_obj_get_user_data(target) - 1;

    if (code == LV_EVENT_PRESSED) {
        uint8_t note = MidiManager::channelButtonNotes[channelIndex];
        uint8_t channel = MidiManager::channelButtonTargetChannel;
        Control_Surface.sendNoteOn({note, (Channel)(channel + 1)}, 127);
    } else if (code == LV_EVENT_RELEASED) {
        uint8_t note = MidiManager::channelButtonNotes[channelIndex];
        uint8_t channel = MidiManager::channelButtonTargetChannel;
        Control_Surface.sendNoteOff({note, (Channel)(channel + 1)}, 0);
    } else if (code == LV_EVENT_CLICKED) {
        if (channelIndex != MidiManager::currentMidiChannel) {
            MidiManager::setMidiChannel(channelIndex);
            if (UiManager::isLfoMode) UiManager::isMixerMode = false;
            UiManager::updateChannelButtonColors();
            UiManager::updatePageButtonColors();
            UiManager::updateLFOButtonColors();
            UiManager::loadValuesForCurrentState();
            UiManager::updateParameterLabels();
        }
    }
}

static void ui_event_ButtonPageGeneric(lv_event_t *e) {
    if (lv_event_get_code(e) == LV_EVENT_CLICKED) {
        int page = (int)(intptr_t)lv_obj_get_user_data(lv_event_get_target(e));
        int newPage = page - 1;
        if (newPage != MidiManager::currentPage) {
            MidiManager::setPage(newPage);
            if (UiManager::isLfoMode) UiManager::isMixerMode = false; 
            UiManager::updatePageButtonColors();
            UiManager::updateLFOButtonColors();
            UiManager::loadValuesForCurrentState();
            UiManager::updateParameterLabels();
        }
    }
}

static void ui_event_ButtonArcGeneric(lv_event_t *e) {
    if (lv_event_get_code(e) == LV_EVENT_CLICKED) {
        int index = (int)(intptr_t)lv_obj_get_user_data(lv_event_get_target(e));
        UiManager::setActiveArc(index);
        UiManager::updateModulationUIColors();
    }
}

static void ui_event_ButtonArcModulationSelect(lv_event_t *e) {
    if (lv_event_get_code(e) == LV_EVENT_LONG_PRESSED) {
        int index = (int)(intptr_t)lv_obj_get_user_data(lv_event_get_target(e));
        UiManager::setModulationTarget(index);
    }
}

static void ui_event_ArcGeneric(lv_event_t *e) {}

static void lfo1EventHandler(lv_event_t *e) { 
    if (lv_event_get_code(e) == LV_EVENT_CLICKED) { 
        if (UiManager::currentMenuState == UiManager::MENU_KEYBOARD) {
            if (UiManager::keyboardSubmode == UiManager::SUBMODE_CHORD) {
                UiManager::chordOctave--;
                if (UiManager::chordOctave < -2) UiManager::chordOctave = -2;
            } else {
                MidiManager::keyboardOctave--;
                if (MidiManager::keyboardOctave < -4) MidiManager::keyboardOctave = -4;
            }
            UiManager::updateLFOButtonColors();
            UiManager::updateParameterLabels();
            return;
        }
        if (UiManager::isShiftActive) { UiManager::setMixerPage(1); return; }
        if (UiManager::isLfoMode) {
            LfoEngine::currentLfoIndex = 0; LfoEngine::mixMode = false;
            UiManager::syncLfoArcValues();
        } else {
            UiManager::setCcRow(0);
        }
        UiManager::updateLFOButtonColors(); 
    } 
}

static void lfo2EventHandler(lv_event_t *e) { 
    if (lv_event_get_code(e) == LV_EVENT_CLICKED) { 
        if (UiManager::currentMenuState == UiManager::MENU_KEYBOARD) {
            if (UiManager::keyboardSubmode == UiManager::SUBMODE_CHORD) {
                UiManager::chordOctave++;
                if (UiManager::chordOctave > 2) UiManager::chordOctave = 2;
            } else {
                MidiManager::keyboardOctave++;
                if (MidiManager::keyboardOctave > 4) MidiManager::keyboardOctave = 4;
            }
            UiManager::updateLFOButtonColors();
            UiManager::updateParameterLabels();
            return;
        }
        if (UiManager::isShiftActive) { UiManager::setMixerPage(2); return; }
        if (UiManager::isLfoMode) {
            LfoEngine::currentLfoIndex = 1; LfoEngine::mixMode = false;
            UiManager::syncLfoArcValues();
        } else {
            UiManager::setCcRow(1);
        }
        UiManager::updateLFOButtonColors(); 
    } 
}

static void lfo3EventHandler(lv_event_t *e) { 
    if (lv_event_get_code(e) == LV_EVENT_CLICKED) { 
        if (UiManager::currentMenuState == UiManager::MENU_KEYBOARD) {
            if (UiManager::keyboardSubmode == UiManager::SUBMODE_CHORD) {
                UiManager::chordOctave = 0;
            } else {
                MidiManager::keyboardOctave = 0;
            }
            UiManager::updateLFOButtonColors();
            return;
        }
        if (UiManager::isShiftActive) { UiManager::setMixerPage(3); return; }
        if (UiManager::isLfoMode) {
            LfoEngine::currentLfoIndex = 2; LfoEngine::mixMode = false;
            UiManager::syncLfoArcValues();
        } else {
            UiManager::setCcRow(2);
        }
        UiManager::updateLFOButtonColors(); 
    } 
}

static void lfo4EventHandler(lv_event_t *e) { 
    if (lv_event_get_code(e) == LV_EVENT_CLICKED) { 
        if (UiManager::currentMenuState == UiManager::MENU_KEYBOARD) {
            // Toggle Keys <-> Chord
            UiManager::keyboardSubmode = (UiManager::keyboardSubmode == UiManager::SUBMODE_KEYS)
                ? UiManager::SUBMODE_CHORD : UiManager::SUBMODE_KEYS;
            UiManager::updateLFOButtonColors();
            UiManager::updateParameterLabels();
            UiManager::updateKeyboardColors();
            return;
        }
        if (UiManager::isShiftActive) { UiManager::setMixerPage(4); return; }
        if (UiManager::isLfoMode) {
            LfoEngine::currentLfoIndex = 3; LfoEngine::mixMode = false;
            UiManager::syncLfoArcValues();
        } else {
            UiManager::setCcRow(3);
        }
        UiManager::updateLFOButtonColors(); 
    } 
}

static void lfoMixEventHandler(lv_event_t *e) { 
    lv_event_code_t code = lv_event_get_code(e);

    if (code == LV_EVENT_PRESSED) {
        if (!UiManager::isLfoMode && UiManager::currentMenuState != UiManager::MENU_KEYBOARD) {
            uint8_t note = UiManager::isMixerMode ? MidiManager::channelButtonNotes[MidiManager::currentMidiChannel] : MidiManager::mixerButtonNote;
            Control_Surface.sendNoteOn({note, (Channel)(MidiManager::channelButtonTargetChannel + 1)}, 127);
        }
    } else if (code == LV_EVENT_RELEASED) {
        if (!UiManager::isLfoMode && UiManager::currentMenuState != UiManager::MENU_KEYBOARD) {
            uint8_t note = UiManager::isMixerMode ? MidiManager::channelButtonNotes[MidiManager::currentMidiChannel] : MidiManager::mixerButtonNote;
            Control_Surface.sendNoteOff({note, (Channel)(MidiManager::channelButtonTargetChannel + 1)}, 0);
        }
    } else if (code == LV_EVENT_CLICKED) { 
        if (UiManager::isShiftActive) { UiManager::setMixerPage(0); return; }

        // In keyboard mode, toggle Keys/Chord submode
        if (UiManager::currentMenuState == UiManager::MENU_KEYBOARD) {
            UiManager::keyboardSubmode = (UiManager::keyboardSubmode == UiManager::SUBMODE_KEYS)
                ? UiManager::SUBMODE_CHORD : UiManager::SUBMODE_KEYS;
            UiManager::updateLFOButtonColors();
            UiManager::updateParameterLabels();
            UiManager::updateKeyboardColors();
            return;
        }

        if (UiManager::isLfoMode) {
            LfoEngine::mixMode = true;
            UiManager::syncLfoArcValues();
        } else {
            UiManager::toggleMixerMode();
        }
        UiManager::updateLFOButtonColors(); 
    } 
}

static void ui_event_ButtonWifi(lv_event_t *e) {
    if (lv_event_get_code(e) == LV_EVENT_CLICKED) {
        if (!WebServerManager::isWiFiEnabled()) WebServerManager::startWiFi();
        else WebServerManager::stopWiFi();
    }
}
