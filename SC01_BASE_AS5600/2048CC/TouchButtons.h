// TouchButtons.h
#ifndef TOUCH_BUTTONS_H
#define TOUCH_BUTTONS_H

#include "Arduino.h"
#include "Config.h"

class UiManager;
class MidiManager;

class TouchButtons {
private:
  static const int NUM_BUTTONS = 2;
  static int buttonPins[NUM_BUTTONS];
  static bool buttonStates[NUM_BUTTONS];
  static bool buttonDetected[NUM_BUTTONS];
  static void (*buttonCallbacks[NUM_BUTTONS])(int, bool);  // callback(pin, isTouched)
  
  static void onTouch(int buttonIndex) {
    buttonDetected[buttonIndex] = true;
  }
  
  static void handleInterrupt1() { onTouch(0); }
  static void handleInterrupt2() { onTouch(1); }
  
  static void (*interruptHandlers[2])();
  
public:
  // Initialize with custom callbacks (optional)
  static void begin(void (*callback)(int pin, bool isTouched) = nullptr) {
    buttonPins[0] = PIN_TOUCH_1;
    buttonPins[1] = PIN_TOUCH_2;
    
    interruptHandlers[0] = handleInterrupt1;
    interruptHandlers[1] = handleInterrupt2;
    
    touchSetDefaultThreshold(6);
    
    for (int i = 0; i < NUM_BUTTONS; i++) {
      buttonStates[i] = false;
      buttonDetected[i] = false;
      buttonCallbacks[i] = callback;
      touchAttachInterrupt(buttonPins[i], interruptHandlers[i], 0);
    }
  }
  
  // Update - call this in your main loop
  static void update() {
    for (int i = 0; i < NUM_BUTTONS; i++) {
      if (buttonDetected[i]) {
        buttonDetected[i] = false;
        bool currentState = touchInterruptGetLastStatus(buttonPins[i]);
        
        if (currentState != buttonStates[i]) {
          buttonStates[i] = currentState;
          
          // Call callback if registered
          if (buttonCallbacks[i] != nullptr) {
            buttonCallbacks[i](buttonPins[i], buttonStates[i]);
          }
        }
      }
    }
  }
  
  // Get current state of a button (true = touched)
  static bool isTouched(int pin) {
    for (int i = 0; i < NUM_BUTTONS; i++) {
      if (buttonPins[i] == pin) return buttonStates[i];
    }
    return false;
  }
  
  // Get current state by index (0 or 1)
  static bool isTouchedIndex(int index) {
    if (index >= 0 && index < NUM_BUTTONS) return buttonStates[index];
    return false;
  }
  
  // Main touch handler — processes pin 12 (shift/mixer/template) and pin 14 (LFO/template)
  static void handleTouch(int pin, bool isTouched);

  // Set custom callback for specific button
  static void setCallback(int pin, void (*callback)(int, bool)) {
    for (int i = 0; i < NUM_BUTTONS; i++) {
      if (buttonPins[i] == pin) {
        buttonCallbacks[i] = callback;
        break;
      }
    }
  }
};

#endif