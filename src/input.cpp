#include "input.h"
#include "config.h"
#include <CST816S.h>
#include "esp_timer.h"
#include "driver/gpio.h"

static CST816S touch(TOUCH_SDA, TOUCH_SCL, TOUCH_RST, TOUCH_IRQ);

#define KNOB_POLL_MS    3
#define KNOB_DEBOUNCE   2

static volatile int encoderDelta = 0;
static uint8_t prevA = 1, prevB = 1;
static uint8_t debounceA = 0, debounceB = 0;

static void IRAM_ATTR knobTimerCb(void* arg) {
    uint8_t a = gpio_get_level((gpio_num_t)ENCODER_A);
    uint8_t b = gpio_get_level((gpio_num_t)ENCODER_B);

    if (a != prevA) {
        debounceA++;
        if (debounceA >= KNOB_DEBOUNCE) {
            debounceA = 0;
            prevA = a;
            if (a == 1) encoderDelta++;
        }
    } else {
        debounceA = 0;
    }

    if (b != prevB) {
        debounceB++;
        if (debounceB >= KNOB_DEBOUNCE) {
            debounceB = 0;
            prevB = b;
            if (b == 1) encoderDelta--;
        }
    } else {
        debounceB = 0;
    }
}

void inputInit() {
    touch.begin();

    gpio_config_t enc_cfg = {};
    enc_cfg.pin_bit_mask = (1ULL << ENCODER_A) | (1ULL << ENCODER_B);
    enc_cfg.mode = GPIO_MODE_INPUT;
    enc_cfg.intr_type = GPIO_INTR_DISABLE;
    enc_cfg.pull_up_en = GPIO_PULLUP_ENABLE;
    gpio_config(&enc_cfg);

    prevA = gpio_get_level((gpio_num_t)ENCODER_A);
    prevB = gpio_get_level((gpio_num_t)ENCODER_B);

    esp_timer_create_args_t timer_args = {};
    timer_args.callback = knobTimerCb;
    timer_args.name = "knob";
    esp_timer_handle_t timer;
    esp_timer_create(&timer_args, &timer);
    esp_timer_start_periodic(timer, KNOB_POLL_MS * 1000);

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
