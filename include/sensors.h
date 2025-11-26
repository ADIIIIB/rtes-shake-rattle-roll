#pragma once

#include "mbed.h"
#include "config.h"

// Single scalar signal per sample that we'll send to FFT (e.g., |accel|)
struct SignalWindow {
    float data[WINDOW_SAMPLES];
    size_t length;  // should be WINDOW_SAMPLES
};

// Initialize IMU + sampling system
bool sensors_init();

// Start periodic sampling at FS_HZ
void sensors_start();

// Non-blocking: returns true when a new 3-second window is ready
bool sensors_get_window(SignalWindow &window);
