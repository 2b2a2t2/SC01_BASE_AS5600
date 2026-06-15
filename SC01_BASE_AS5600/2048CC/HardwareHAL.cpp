#include "HardwareHAL.h"

bool HardwareHAL::vibrationActive = false;
unsigned long HardwareHAL::vibrationStartTime = 0;
int HardwareHAL::currentVibrationDuration = 1;

void HardwareHAL::init() {
    Wire.begin(PIN_SDA, PIN_SCL);
    pinMode(PIN_VIBRATION_MOTOR, OUTPUT);
    digitalWrite(PIN_VIBRATION_MOTOR, LOW);
}

void HardwareHAL::tcaSelect(uint8_t muxAddr, uint8_t channel) {
    if (channel > 7) return;

    // Turn off all other multiplexers to avoid address conflicts if any
    // (Existing logic from ino)
    Wire.beginTransmission(TCA9548A_ADDR_1);
    Wire.write(0x00);
    Wire.endTransmission();
    
    Wire.beginTransmission(TCA9548A_ADDR_2);
    Wire.write(0x00);
    Wire.endTransmission();
    
    Wire.beginTransmission(TCA9548A_ADDR_3);
    Wire.write(0x00);
    Wire.endTransmission();

    // Select the desired multiplexer and channel
    Wire.beginTransmission(muxAddr);
    Wire.write(1 << channel);
    Wire.endTransmission();
}

float HardwareHAL::readAngle() {
    Wire.beginTransmission(AS5600_ADDR);
    Wire.write(0x0C); // RAW ANGLE Register
    Wire.endTransmission();
    
    Wire.requestFrom(AS5600_ADDR, 2);
    if (Wire.available() == 2) {
        uint16_t highbyte = Wire.read();
        uint16_t lowbyte = Wire.read();
        uint16_t rawAngle = (highbyte << 8) | lowbyte;
        return rawAngle * 0.087890625; // 360 / 4096
    }
    return -1;
}

void HardwareHAL::vibrate(int durationMs) {
    digitalWrite(PIN_VIBRATION_MOTOR, HIGH);
    vibrationActive = true;
    vibrationStartTime = millis();
    currentVibrationDuration = durationMs;
}

void HardwareHAL::updateVibration() {
    if (vibrationActive && (millis() - vibrationStartTime >= currentVibrationDuration)) {
        digitalWrite(PIN_VIBRATION_MOTOR, LOW);
        vibrationActive = false;
    }
}
