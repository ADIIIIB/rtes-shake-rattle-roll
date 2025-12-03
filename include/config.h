#pragma once
#include <cstddef>
#include <cstdint>

// Sampling configuration
constexpr float  FS_HZ        = 52.0f;    // IMU sampling rate
constexpr float  WINDOW_SEC   = 3.0f;     // analysis window length (seconds)
constexpr size_t WINDOW_SAMPLES =
    static_cast<size_t>(FS_HZ * WINDOW_SEC);  // 156 samples

// FFT configuration
constexpr size_t FFT_SIZE = 256;   // power of 2 >= WINDOW_SAMPLES

// Frequency bands (tunable)
constexpr float TREMOR_F_LOW   = 3.0f;   // Hz
constexpr float TREMOR_F_HIGH  = 7.0f;   // Hz
constexpr float DYSK_F_LOW     = 7.0f;   // Hz
constexpr float DYSK_F_HIGH    = 15.0f;  // Hz

// Simple thresholds (tunable)
constexpr float MIN_TOTAL_POWER     = 1e-3f;  // ignore almost-no-motion windows
constexpr float MIN_RELATIVE_ENERGY = 0.3f;   // 30% of energy in band

// Shared window type
struct SignalWindow {
    float  data[WINDOW_SAMPLES];
    size_t length;
};