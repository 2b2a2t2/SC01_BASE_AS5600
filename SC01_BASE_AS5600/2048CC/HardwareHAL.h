#ifndef HARDWARE_HAL_H
#define HARDWARE_HAL_H

#include <Arduino.h>
#include <Wire.h>
#include "Config.h"

class HardwareHAL {
public:
    static void init();
    
    // I2C Multiplexer Control
    static void tcaSelect(uint8_t muxAddr, uint8_t channel);
    
    // AS5600 Sensor Reading
    static float readAngle();
    
    // Vibration Motor Control
    static void vibrate(int durationMs = VIBRATION_DURATION_MS);
    static void updateVibration(); // Call in loop to handle non-blocking vibration

private:
    static bool vibrationActive;
    static unsigned long vibrationStartTime;
    static int currentVibrationDuration;

    // Cached mux state to avoid redundant I2C writes
    static uint8_t currentMuxAddr;
    static uint8_t currentMuxChannel;
};

#endif // HARDWARE_HAL_H
