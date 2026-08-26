#include "input.h"
#include "config.h"
#include <CST816S.h>
#include "esp_timer.h"

static CST816S touch(TOUCH_SDA, TOUCH_SCL, TOUCH_RST, TOUCH_IRQ);

static volatile int encoderDelta = 0;

static void IRAM_ATTR encoderISR() {
    static unsigned long lastISR = 0;
    unsigned long now = millis();
    if (now - lastISR < 5) return;
    lastISR = now;
    ets_delay_us(80);
    if (digitalRead(ENCODER_B)) encoderDelta++;
    else encoderDelta--;
}

void inputInit() {
    touch.begin();

    pinMode(ENCODER_A, INPUT_PULLUP);
    pinMode(ENCODER_B, INPUT_PULLUP);
    attachInterrupt(digitalPinToInterrupt(ENCODER_A), encoderISR, FALLING);

    Serial.println("[input] OK");
}

InputAction inputPoll() {
    int delta = encoderDelta;
    if (delta != 0) {
        encoderDelta = 0;
        if (delta > 0) return INPUT_KNOB_CW;
        return INPUT_KNOB_CCW;
    }

    if (touch.available()) {
        switch (touch.data.gestureID) {
            case 1: return INPUT_SWIPE_UP;
            case 2: return INPUT_SWIPE_DOWN;
            case 3: return INPUT_SWIPE_LEFT;
            case 4: return INPUT_SWIPE_RIGHT;
            case 5: return INPUT_TAP;
            default: break;
        }
    }
    return INPUT_NONE;
}
