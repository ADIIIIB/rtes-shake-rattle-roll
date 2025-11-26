#pragma once
#include "sensors.h"

// 0 = none, 1 = freeze start, 2 = sustained freeze
struct GaitStatus {
    uint8_t fog_state;
};

void gait_init();
GaitStatus gait_update(const SignalWindow &window);
