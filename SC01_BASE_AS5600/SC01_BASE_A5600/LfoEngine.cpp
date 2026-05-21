#include "LfoEngine.h"
#include "Config.h"

// --- LfoUnit Implementation ---

LfoUnit::LfoUnit() : 
    depth(0.5), rate(1.0), shape(LFO_SINE), offset(0.5), phase(0), value(0),
    waveformIndex(0), lastUpdate(0), sampleHoldValue(0), lastSampleHoldTime(0),
    randomValue(0), lastRandomTime(0), canvas(nullptr), initialized(false) 
{
    for(int i=0; i<50; i++) waveformBuffer[i] = 0.5f;
}

void LfoUnit::init() {
    lastUpdate = millis();
    lastSampleHoldTime = millis();
    lastRandomTime = millis();
}

float LfoUnit::calculateValueAt(float p) {
    float val = 0;
    float t = fmod(p, 1.0f);

    switch (shape) {
        case LFO_SINE:
            val = sin(t * 2.0f * PI);
            break;
        case LFO_TRIANGLE:
            if (t < 0.5f) val = (4.0f * t - 1.0f);
            else val = (-4.0f * t + 3.0f);
            break;
        case LFO_SAW_UP:
            val = 2.0f * t - 1.0f;
            break;
        case LFO_SAW_DOWN:
            val = 1.0f - 2.0f * t;
            break;
        case LFO_SQUARE:
            val = (t < 0.5f) ? 1.0f : -1.0f;
            break;
        case LFO_RANDOM: {
            unsigned long now = millis();
            int interval = (int)(1000.0f / (rate * 2.0f));
            if (interval < 5) interval = 5;
            if (now - lastRandomTime >= (unsigned long)interval) {
                randomValue = ((float)random(2000) / 1000.0f) - 1.0f;
                lastRandomTime = now;
            }
            val = randomValue;
            break;
        }
        case LFO_SAMPLE_HOLD: {
            unsigned long now = millis();
            int interval = (int)(1000.0f / rate);
            if (interval < 5) interval = 5;
            if (now - lastSampleHoldTime >= (unsigned long)interval) {
                sampleHoldValue = ((float)random(2000) / 1000.0f) - 1.0f;
                lastSampleHoldTime = now;
            }
            val = sampleHoldValue;
            break;
        }
    }

    float d = depth * MAX_LFO_DEPTH;
    float o = (offset - 0.5f) * 5.0f;
    float bipolar = (val * d) + o;
    bipolar = constrain(bipolar, -1.0f, 1.0f);
    return (bipolar * 0.5f) + 0.5f;
}

void LfoUnit::update(float dt) {
    phase += rate * dt;
    if (phase >= 1.0f) phase -= 1.0f;

    value = calculateValueAt(phase);
    
    waveformBuffer[waveformIndex] = value;
    waveformIndex = (waveformIndex + 1) % 50;
}

void LfoUnit::updateDisplay(lv_obj_t* parentButton) {
    if (!parentButton) return;

    if (!initialized) {
        canvas = lv_canvas_create(parentButton);
        lv_obj_set_size(canvas, 60, 40);
        lv_obj_align(canvas, LV_ALIGN_CENTER, 0, 0);
        lv_canvas_set_buffer(canvas, canvas_buffer, 60, 40, LV_IMG_CF_TRUE_COLOR);
        lv_draw_line_dsc_init(&line_dsc);
        line_dsc.color = lv_color_hex(0x94AA00);
        line_dsc.width = 1;
        line_dsc.round_start = 1;
        line_dsc.round_end = 1;
        initialized = true;
    }

    lv_canvas_fill_bg(canvas, lv_color_hex(0x000000), LV_OPA_COVER);

    lv_point_t points[50];
    for (int i = 0; i < 50; i++) {
        int idx = (waveformIndex + i) % 50;
        float v = waveformBuffer[idx];
        points[i].x = i * 60 / 50;
        float bipolar = (v - 0.5f) * 2.0f;
        points[i].y = 20 - (int)(bipolar * 18);
    }
    lv_canvas_draw_line(canvas, points, 50, &line_dsc);
    lv_obj_invalidate(canvas);
}

// --- LfoEngine Implementation ---

LfoUnit LfoEngine::lfos[4];
int LfoEngine::currentLfoIndex = 0;
bool LfoEngine::mixMode = false;
float LfoEngine::mixAmounts[4] = { 1.0f, 0.0f, 0.0f, 0.0f };
float LfoEngine::savedMixAmounts[4] = { 1.0f, 0.0f, 0.0f, 0.0f };
bool LfoEngine::isKilled = false;
float LfoEngine::mainWaveformBuffer[100] = { 0 };
int LfoEngine::mainWaveformIndex = 0;

void LfoEngine::init() {
    for(int i=0; i<4; i++) {
        lfos[i].init();
    }
}

void LfoEngine::update() {
    static unsigned long lastUpdate = 0;
    unsigned long now = millis();
    if (lastUpdate == 0) lastUpdate = now;
    
    if (now - lastUpdate >= 10) {
        float dt = (now - lastUpdate) / 1000.0f;
        lastUpdate = now;
        for(int i=0; i<4; i++) {
            lfos[i].update(dt);
        }
        addToMainBuffer(getMixedValue());
    }
}

float LfoEngine::getMixedValue() {
    if (isKilled) return 0.5f;
    float bipolarMix = 0;
    for (int i = 0; i < 4; i++) {
        // Convert 0..1 to -1..1 for bipolar mixing
        float bipolar = (lfos[i].getCurrentValue() - 0.5f) * 2.0f;
        bipolarMix += bipolar * mixAmounts[i];
    }
    bipolarMix = constrain(bipolarMix, -1.0f, 1.0f);
    return (bipolarMix * 0.5f) + 0.5f;
}

void LfoEngine::addToMainBuffer(float val) {
    mainWaveformBuffer[mainWaveformIndex] = val;
    mainWaveformIndex = (mainWaveformIndex + 1) % 100;
}
