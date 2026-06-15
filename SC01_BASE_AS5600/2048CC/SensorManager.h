#ifndef SENSOR_MANAGER_H
#define SENSOR_MANAGER_H

#include <Arduino.h>
#include "Config.h"

class SensorManager {
public:
    static void init();
    static void update();
    
    static float getPotValue(int index);
    static int getMidiCCValue(int index);
    
    // Shared state (moved from globals)
    static float previousAngles[20];
    static float potentiometerValues[20];
    static int currentMidiCCValues[NUM_POTS];
    static float currentPotentiometerValues[NUM_POTS];

private:
    static void updateStandardPot(int i, float currentAngle, float angleChange);
    static void updateLfoPot(int i, float currentAngle, float angleChange);
    static void updateMixerPot(int i, float currentAngle, float angleChange);
    static void updateRowCCPot(int i, float currentAngle, float angleChange);
};

#endif // SENSOR_MANAGER_H
