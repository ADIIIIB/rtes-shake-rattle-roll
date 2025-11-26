#include "sensors.h"
#include <cstddef>
// ==== IMU DRIVER PLACEHOLDER ====
// TODO: replace with actual IMU driver from your recitation code.
// Example: include LSM6DSL driver and create an object here.

static Ticker sample_ticker;
static volatile bool sample_flag = false;

// Circular buffer for moving average
static float ma_buffer[WINDOW_SAMPLES];
static size_t ma_head = 0;
static float ma_sum = 0.0f;
static bool   buffer_filled = false;

// Storage for the last 3-second window to give to FFT
static SignalWindow last_window;
static volatile bool window_ready = false;

// Called by hardware timer
static void sample_isr() {
    sample_flag = true;
}

// Replace this with real IMU reading
static bool read_imu_accel(float &ax, float &ay, float &az) {
    // TODO: implement using board's accelerometer driver
    // For now, return zeros so project builds & runs.
    ax = ay = az = 0.0f;
    return true;
}

static float compute_magnitude(float ax, float ay, float az) {
    return sqrtf(ax * ax + ay * ay + az * az);
}

bool sensors_init() {
    // TODO: init IMU here (I2C/SPI setup + sensor config)
    // return false if it fails.
    return true;
}

void sensors_start() {
    // Clear state
    ma_head = 0;
    ma_sum = 0.0f;
    buffer_filled = false;
    window_ready = false;

    // Attach ticker at 1 / FS_HZ seconds
    const float period_s = 1.0f / FS_HZ;
    sample_ticker.attach(&sample_isr, period_s);
}

bool sensors_get_window(SignalWindow &window) {
    if (!sample_flag) {
        return false;
    }
    sample_flag = false;

    // 1. Read IMU
    float ax, ay, az;
    if (!read_imu_accel(ax, ay, az)) {
        return false; // read failed
    }

    // 2. Convert to magnitude
    float mag = compute_magnitude(ax, ay, az);

    // 3. Moving average via circular buffer (O(1))
    float old = ma_buffer[ma_head];
    ma_sum -= old;
    ma_buffer[ma_head] = mag;
    ma_sum += mag;

    ma_head++;
    if (ma_head >= WINDOW_SAMPLES) {
        ma_head = 0;
        buffer_filled = true;
    }

    // Only output full windows once buffer is filled
    if (!buffer_filled) {
        return false;
    }

    // Build a time-ordered window starting at head index
    for (size_t i = 0; i < WINDOW_SAMPLES; ++i) {
        size_t idx = (ma_head + i) % WINDOW_SAMPLES;
        last_window.data[i] = ma_buffer[idx];
    }
    last_window.length = WINDOW_SAMPLES;

    window = last_window;
    return true;
}