#include "gait.h"
#include "parkinsons_system.h"
#include <cmath>

// State tracking for FOG detection
static float prev_variance = 0.0f;

void gait_init() {
    prev_variance = 0.0f;
}

GaitStatus gait_update(const SignalWindow &window) {
    GaitStatus status{};

    // Calculate magnitude for each sample
    float magnitude_sum = 0.0f;
    float magnitude[BUFFER_SIZE];

    for (int i = 0; i < BUFFER_SIZE; i++) {
        magnitude[i] = sqrt_custom(
            sensor_data.accel_x[i] * sensor_data.accel_x[i] +
            sensor_data.accel_y[i] * sensor_data.accel_y[i] +
            sensor_data.accel_z[i] * sensor_data.accel_z[i]
        );
        magnitude_sum += magnitude[i];
    }

    float mean_magnitude = magnitude_sum / BUFFER_SIZE;

    // Calculate variance (motion regularity)
    float variance = 0.0f;
    for (int i = 0; i < BUFFER_SIZE; i++) {
        float diff = magnitude[i] - mean_magnitude;
        variance += diff * diff;
    }
    variance /= BUFFER_SIZE;
    float std_dev = sqrt_custom(variance);

    // FOG Detection Logic based on variance and mean magnitude
    // Low variance + low acceleration = freezing of gait
    // Normal gait: acceleration magnitude 0.8-1.5g with high variance
    // FOG state: acceleration magnitude < 0.8g with very low variance

    // Thresholds tuned for Parkinson's freezing detection
    float low_motion_threshold = 0.8f;    // Below normal gravity + gait
    float variance_threshold_low = 0.25f; // Very rigid motion
    float variance_threshold_high = 0.15f; // Extremely rigid (frozen)

    // Condition 1: Very low motion with very low variance = FREEZE
    if (mean_magnitude < low_motion_threshold && std_dev < variance_threshold_high) {
        if (prev_variance > variance_threshold_low) {
            status.fog_state = 1;  // Freeze start (sudden drop to stillness)
        } else {
            status.fog_state = 2;  // Sustained freeze
        }
    }
    // Condition 2: Some motion but extremely rigid = FREEZE (tremor/rigidity)
    else if (std_dev < variance_threshold_high) {
        status.fog_state = 2;  // Rigid tremorous movement
    }
    else {
        status.fog_state = 0;  // Normal gait
    }

    // Debug: Uncomment to see FOG detection values
    // printf("  [FOG] Mean:%.3f StdDev:%.3f PrevVar:%.3f State:%d\r\n",
    //        mean_magnitude, std_dev, prev_variance, status.fog_state);

    prev_variance = std_dev;
    return status;
}
