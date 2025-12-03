#include "dsp.h"
#include <cmath>

// No special init needed for the simple DFT
void dsp_init() {}

// Map [value, max_value] to 0–100 (clamped)
static uint8_t scale_to_100(float value, float max_value) {
    if (max_value <= 0.0f) return 0;
    float r = value / max_value;
    if (r < 0.0f) r = 0.0f;
    if (r > 1.0f) r = 1.0f;
    return static_cast<uint8_t>(r * 100.0f + 0.5f);
}

// Compute power in [f_low, f_high] using a naive DFT
static float band_power_dft(const SignalWindow &window,
                            float fs,
                            float f_low,
                            float f_high)
{
    const size_t N = window.length;
    if (N == 0) return 0.0f;

    const float TWO_PI = 6.28318530717958647692f;
    const float df = fs / N;

    int k_low  = static_cast<int>(std::ceil(f_low  / df));
    int k_high = static_cast<int>(std::floor(f_high / df));

    if (k_low < 1) k_low = 1;
    if (k_high > static_cast<int>(N/2)) {
        k_high = static_cast<int>(N/2);
    }
    if (k_high < k_low) return 0.0f;

    float sum_power = 0.0f;

    // For each frequency bin k, compute DFT coefficient X[k]
    for (int k = k_low; k <= k_high; ++k) {
        float re = 0.0f;
        float im = 0.0f;

        for (size_t n = 0; n < N; ++n) {
            float angle = TWO_PI * k * n / N;
            float x = window.data[n];
            re += x * std::cos(angle);
            im -= x * std::sin(angle);
        }

        float mag2 = re * re + im * im;  // magnitude squared
        sum_power += mag2;
    }

    // Normalize a bit so numbers are not gigantic
    return sum_power / (N * N);
}

MovementAnalysis dsp_analyze_window(const SignalWindow &window) {
    MovementAnalysis result{0, 0};

    const size_t N = window.length;
    if (N == 0) {
        return result;
    }

    // Total power in 0.5–15 Hz
    float total_power = band_power_dft(window, FS_HZ, 0.5f, 15.0f);
    if (total_power < MIN_TOTAL_POWER) {
        // almost no motion
        return result;
    }

    float tremor_power = band_power_dft(window, FS_HZ, TREMOR_F_LOW, TREMOR_F_HIGH);
    float dysk_power   = band_power_dft(window, FS_HZ, DYSK_F_LOW,   DYSK_F_HIGH);

    float tremor_rel = tremor_power / total_power;
    float dysk_rel   = dysk_power   / total_power;

    if (tremor_rel > MIN_RELATIVE_ENERGY) {
        // Compress the 0–1 range so a pure tone doesn't always become 100
        float rel = tremor_power / total_power;   // 0..1
        float compressed = std::sqrt(rel);        // still 0..1, but softer
        result.tremor_level = static_cast<uint8_t>(compressed * 80.0f + 0.5f);
    }
    if (dysk_rel > MIN_RELATIVE_ENERGY) {
        result.dyskinesia_level = scale_to_100(dysk_power, total_power);
    }

    return result;
}