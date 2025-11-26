#pragma once

#include "config.h"
#include "sensors.h"

// Output of the FFT analysis for 1 window
struct MovementAnalysis {
    uint8_t tremor_level;      // 0–100
    uint8_t dyskinesia_level;  // 0–100
};

void dsp_init();

// Analyze one window and return tremor/dyskinesia levels
MovementAnalysis dsp_analyze_window(const SignalWindow &window);
