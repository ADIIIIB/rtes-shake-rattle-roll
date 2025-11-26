#include "dsp.h"
#include "arm_math.h"          // CMSIS-DSP


static arm_rfft_fast_instance_f32 rfft_instance;

// Utility: map [value, max_value] to 0–100 (clamped)
static uint8_t scale_to_100(float value, float max_value) {
    if (max_value <= 0.0f) return 0;
    float r = value / max_value;
    if (r < 0.0f) r = 0.0f;
    if (r > 1.0f) r = 1.0f;
    return static_cast<uint8_t>(r * 100.0f + 0.5f);
}

void dsp_init() {
    arm_rfft_fast_init_f32(&rfft_instance, FFT_SIZE);
}

// Compute sum of magnitude spectrum in [f_low, f_high]
static float band_power(const float *mag, float fs, float f_low, float f_high) {
    const float df = fs / FFT_SIZE; // frequency resolution
    int k_low  = static_cast<int>(ceilf(f_low  / df));
    int k_high = static_cast<int>(floorf(f_high / df));
    if (k_low < 1) k_low = 1;
    if (k_high > static_cast<int>(FFT_SIZE/2)) k_high = FFT_SIZE/2;

    float sum = 0.0f;
    for (int k = k_low; k < k_high; ++k) {
        sum += mag[k];
    }
    return sum;
}

MovementAnalysis dsp_analyze_window(const SignalWindow &window) {
    MovementAnalysis result{0, 0};

    // 1. Prepare FFT input with zero-padding
    static float fft_in[FFT_SIZE];
    static float fft_out[FFT_SIZE]; // complex output interleaved

    for (size_t i = 0; i < FFT_SIZE; ++i) {
        if (i < window.length) {
            // Optionally apply a window (Hann, etc.) here
            fft_in[i] = window.data[i];
        } else {
            fft_in[i] = 0.0f;
        }
    }

    // 2. Run real FFT
    arm_rfft_fast_f32(&rfft_instance, fft_in, fft_out, 0);

    // 3. Convert to magnitude spectrum (bins 0..FFT_SIZE/2)
    static float mag[FFT_SIZE/2 + 1];
    mag[0] = fabsf(fft_out[0]); // DC component
    for (size_t k = 1; k <= FFT_SIZE/2; ++k) {
        float re = fft_out[2*k];
        float im = fft_out[2*k + 1];
        mag[k] = sqrtf(re * re + im * im);
    }

    // 4. Compute band powers
    float total_power = band_power(mag, FS_HZ, 0.5f, 15.0f);
    if (total_power < MIN_TOTAL_POWER) {
        // almost no motion
        return result;
    }

    float tremor_power = band_power(mag, FS_HZ, TREMOR_F_LOW, TREMOR_F_HIGH);
    float dysk_power   = band_power(mag, FS_HZ, DYSK_F_LOW,   DYSK_F_HIGH);

    float tremor_rel = tremor_power / total_power;
    float dysk_rel   = dysk_power   / total_power;

    if (tremor_rel > MIN_RELATIVE_ENERGY) {
        result.tremor_level = scale_to_100(tremor_power, total_power);
    }
    if (dysk_rel > MIN_RELATIVE_ENERGY) {
        result.dyskinesia_level = scale_to_100(dysk_power, total_power);
    }

    return result;
}