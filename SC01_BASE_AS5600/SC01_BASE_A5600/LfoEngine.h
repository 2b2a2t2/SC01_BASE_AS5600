#ifndef LFO_ENGINE_H
#define LFO_ENGINE_H

#include <Arduino.h>
#include <lvgl.h>

enum LfoShape {
    LFO_SINE = 0,
    LFO_TRIANGLE,
    LFO_SAW_UP,
    LFO_SAW_DOWN,
    LFO_SQUARE,
    LFO_RANDOM,
    LFO_SAMPLE_HOLD,
    NUM_LFO_SHAPES
};

class LfoUnit {
public:
    LfoUnit();
    void init();
    void update(float dt);
    float calculateValueAt(float phase);
    float getCurrentValue() const { return value; }
    
    // UI Integration
    void updateDisplay(lv_obj_t* parentButton);

    // Parameters
    float depth;
    float rate;
    int shape;
    float offset;
    float phase;
    float value;
    
    float waveformBuffer[50];
    int waveformIndex;

private:
    unsigned long lastUpdate;
    float sampleHoldValue;
    unsigned long lastSampleHoldTime;
    float randomValue;
    unsigned long lastRandomTime;
    
    // LVGL objects
    lv_obj_t *canvas;
    lv_draw_line_dsc_t line_dsc;
    lv_color_t canvas_buffer[60 * 40];
    bool initialized;
};

class LfoEngine {
public:
    static void init();
    static void update();
    static float getMixedValue();
    
    static LfoUnit lfos[4];
    static int currentLfoIndex;
    static bool mixMode;
    static float mixAmounts[4];
    static float savedMixAmounts[4];
    static bool isKilled;
    
    static const int MAIN_WAVEFORM_BUFFER_SIZE = 100;
    static float mainWaveformBuffer[MAIN_WAVEFORM_BUFFER_SIZE];
    static int mainWaveformIndex;
    
    static void addToMainBuffer(float val);
};

#endif // LFO_ENGINE_H
