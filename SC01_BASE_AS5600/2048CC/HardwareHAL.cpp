#include "HardwareHAL.h"

bool HardwareHAL::vibrationActive = false;
unsigned long HardwareHAL::vibrationStartTime = 0;
int HardwareHAL::currentVibrationDuration = 1;
uint8_t HardwareHAL::currentMuxAddr = 0xFF;
uint8_t HardwareHAL::currentMuxChannel = 0xFF;

void HardwareHAL::init() {
    Wire.begin(PIN_SDA, PIN_SCL, 400000);
    pinMode(PIN_VIBRATION_MOTOR, OUTPUT);
    digitalWrite(PIN_VIBRATION_MOTOR, LOW);
    currentMuxAddr = 0xFF;
    currentMuxChannel = 0xFF;
}

void HardwareHAL::tcaSelect(uint8_t muxAddr, uint8_t channel) {
    if (channel > 7) return;

    // Cache hit — same mux and channel already active
    if (muxAddr == currentMuxAddr && channel == currentMuxChannel) return;

    // Switching to a different mux — disable the old one first
    if (muxAddr != currentMuxAddr && currentMuxAddr != 0xFF) {
        Wire.beginTransmission(currentMuxAddr);
        Wire.write(0x00);
        Wire.endTransmission();
    }

    // Select the desired multiplexer and channel
    Wire.beginTransmission(muxAddr);
    Wire.write(1 << channel);
    Wire.endTransmission();

    currentMuxAddr = muxAddr;
    currentMuxChannel = channel;
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
