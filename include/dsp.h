#pragma once
#include <cstdint>
#include "config.h"

struct MovementAnalysis {
    uint8_t tremor_level;      // 0–100
    uint8_t dyskinesia_level;  // 0–100 (unused for now)
};

void dsp_init();
MovementAnalysis dsp_analyze_window(const SignalWindow &window);
