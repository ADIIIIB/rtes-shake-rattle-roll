#pragma once
#include <cstdint>
#include "config.h"

struct GaitStatus {
    uint8_t fog_state;      // 0 = none, 1 = possible FOG
    uint8_t step_rate_spm;  // steps per minute (approx, 0–255)
    uint8_t variability;    // 0–100: gait variability proxy
};

void gait_init();
GaitStatus gait_update(const SignalWindow &window);