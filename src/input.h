#pragma once

enum InputAction {
    INPUT_NONE = 0,
    INPUT_TAP,
    INPUT_SWIPE_UP,
    INPUT_SWIPE_DOWN,
    INPUT_SWIPE_LEFT,
    INPUT_SWIPE_RIGHT,
    INPUT_KNOB_CW,
    INPUT_KNOB_CCW,
};

void        inputInit();
InputAction inputPoll();
