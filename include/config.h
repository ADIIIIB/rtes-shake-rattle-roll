#pragma once
#include <cstddef>

// Sampling configuration
constexpr float  FS_HZ        = 52.0f;        // IMU sampling rate
constexpr float  WINDOW_SEC   = 3.0f;         // analysis window length
constexpr size_t WINDOW_SAMPLES = static_cast<size_t>(FS_HZ * WINDOW_SEC); // 156

// FFT configuration (power of two ≥ WINDOW_SAMPLES)
constexpr size_t FFT_SIZE     = 256;          // zero padding up to 256

// Frequency bands (Hz)
constexpr float TREMOR_F_LOW  = 3.0f;
constexpr float TREMOR_F_HIGH = 5.0f;
constexpr float DYSK_F_LOW    = 5.0f;
constexpr float DYSK_F_HIGH   = 7.0f;

// Simple thresholds (you will tune these)
constexpr float MIN_TOTAL_POWER      = 1e-3f;  // ignore windows with almost no motion
constexpr float MIN_RELATIVE_ENERGY  = 0.3f;   // 30% of energy in band to count as tremor/dysk
