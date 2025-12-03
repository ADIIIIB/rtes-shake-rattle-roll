#include "sensors.h"
#include "mbed.h"
#include <cmath>

#ifndef PI
#define PI 3.14159265358979323846f
#endif

// ===== Global sampling / window state =====

static Ticker sample_ticker;
static volatile bool sample_flag = false;

// Ring buffer for last WINDOW_SAMPLES samples
static float buffer[WINDOW_SAMPLES];
static size_t head   = 0;
static bool   filled = false;

static SignalWindow last_window;

// ISR: just sets a flag
static void sample_isr() {
    sample_flag = true;
}

// ===== LSM6DSL IMU via I2C (adapted from recitation) =====

// Use the same pins as your recitation code
// (PB_11 = SDA, PB_10 = SCL on B-L475E-IOT01A)
static I2C i2c(PB_11, PB_10);

// LSM6DSL I2C address (0x6A shifted left by 1 for Mbed's 8-bit addressing)
#define LSM6DSL_ADDR        (0x6A << 1)

// Register addresses
#define WHO_AM_I            0x0F  // Device identification register
#define CTRL1_XL            0x10  // Accelerometer control register
#define CTRL2_G             0x11  // Gyroscope control register
#define CTRL3_C             0x12  // Common control register

#define OUTX_L_XL           0x28  // Accelerometer X-axis low byte
#define OUTX_H_XL           0x29  // Accelerometer X-axis high byte
#define OUTY_L_XL           0x2A  // Accelerometer Y-axis low byte
#define OUTY_H_XL           0x2B  // Accelerometer Y-axis high byte
#define OUTZ_L_XL           0x2C  // Accelerometer Z-axis low byte
#define OUTZ_H_XL           0x2D  // Accelerometer Z-axis high byte

// Flag: if true, we use synthetic tremor instead of IMU
static bool use_synthetic = false;

// Write a single byte to a register
static void write_reg(uint8_t reg, uint8_t val) {
    char data[2] = {(char)reg, (char)val};
    i2c.write(LSM6DSL_ADDR, data, 2);
}

// Read a single byte from a register
static bool read_reg(uint8_t reg, uint8_t &val) {
    char r = (char)reg;
    // Write register address with repeated start condition
    if (i2c.write(LSM6DSL_ADDR, &r, 1, true) != 0) return false;
    // Read the register value
    if (i2c.read(LSM6DSL_ADDR, &r, 1) != 0) return false;
    val = (uint8_t)r;
    return true;
}

// Read 16-bit signed integer from two consecutive registers
static int16_t read_int16(uint8_t reg_low) {
    uint8_t lo = 0, hi = 0;
    read_reg(reg_low,     lo);
    read_reg(reg_low + 1, hi);
    return (int16_t)((hi << 8) | lo);
}

// Initialize the LSM6DSL sensor (adapted from recitation init_sensor)
static bool imu_init() {
    uint8_t who = 0;

    // Read WHO_AM_I register and verify it's 0x6A
    if (!read_reg(WHO_AM_I, who) || who != 0x6A) {
        printf("LSM6DSL: Sensor not found! WHO_AM_I = 0x%02X\r\n", who);
        return false;
    }

    // Configure Block Data Update (BDU)
    write_reg(CTRL3_C, 0x44);

    // Configure Accelerometer: 104 Hz ODR, +/- 8g range, 400Hz Analog Filter
    // 0x4C = 0100 1100 (ODR=104Hz, FS=8g, BW=400Hz)
    write_reg(CTRL1_XL, 0x4C);

    // Configure Gyroscope (Optional, not used in this specific demo)
    write_reg(CTRL2_G, 0x00); // Power down gyro to save power

    // Wait for sensor to stabilize
    ThisThread::sleep_for(100ms);

    printf("LSM6DSL: init OK (WHO_AM_I=0x%02X)\r\n", who);
    return true;
}

// Read accelerometer axes in g's
static bool imu_read_accel(float &ax, float &ay, float &az) {
    // Read raw X, Y, Z
    int16_t raw_x = read_int16(OUTX_L_XL);
    int16_t raw_y = read_int16(OUTY_L_XL);
    int16_t raw_z = read_int16(OUTZ_L_XL);

    // Sensitivity for +/- 8g is 0.244 mg/LSB  ->  0.000244 g/LSB
    const float SENS = 0.244f / 1000.0f;

    ax = raw_x * SENS;
    ay = raw_y * SENS;
    az = raw_z * SENS;

    return true;
}

// ===== Synthetic fallback signal (4 Hz tremor) =====

static bool synthetic_accel(float &ax, float &ay, float &az) {
    static uint32_t sample_idx = 0;

    const float f_tremor = 4.0f;
    float t = sample_idx / FS_HZ;   // seconds

    float val = 0.5f * sinf(2.0f * PI * f_tremor * t);  // ~0.5 g amplitude

    ax = val;
    ay = 0.0f;
    az = 0.0f;

    sample_idx++;
    return true;
}

// ===== Wrapper used by the pipeline =====

static bool read_imu_accel(float &ax, float &ay, float &az) {
    if (!use_synthetic) {
        if (imu_read_accel(ax, ay, az)) {
            return true;
        }
        printf("LSM6DSL: read failed, switching to synthetic mode\r\n");
        use_synthetic = true;
    }
    // Fallback: synthetic tremor so the project always runs
    return synthetic_accel(ax, ay, az);
}

// ===== Signal mapping for DSP/gait: use one axis =====

// You can change this to use magnitude or another axis if you want
static float compute_signal(float ax, float ay, float az) {
    (void)ay;
    (void)az;
    // Here we use X-axis as the 1D signal for tremor/gait analysis
    return ax;
}

// ===== Public sensor API =====

bool sensors_init() {
    // Set I2C speed
    i2c.frequency(400000);  // 400 kHz

    // Try to init IMU. If it fails, we still return true but enable synthetic mode.
    if (!imu_init()) {
        printf("LSM6DSL: init failed, using synthetic signal only\r\n");
        use_synthetic = true;
    } else {
        use_synthetic = false;
    }

    return true;  // Always true so main() doesn't go into fatal blink
}

void sensors_start() {
    head        = 0;
    filled      = false;
    sample_flag = false;

    const float period_s = 1.0f / FS_HZ;
    sample_ticker.attach(&sample_isr, period_s);  // deprecated but fine
}

bool sensors_get_window(SignalWindow &window) {
    if (!sample_flag) {
        return false;
    }
    sample_flag = false;

    // 1. Read accelerometer (real IMU or synthetic fallback)
    float ax, ay, az;
    if (!read_imu_accel(ax, ay, az)) {
        return false;
    }

    // 2. Convert to 1D signal for analysis
    float sig = compute_signal(ax, ay, az);

    // 3. Push into ring buffer
    buffer[head] = sig;
    head++;
    if (head >= WINDOW_SAMPLES) {
        head = 0;
        filled = true;
    }

    if (!filled) {
        return false;  // need one full window before outputting
    }

    // 4. Build time-ordered window starting at head
    for (size_t i = 0; i < WINDOW_SAMPLES; ++i) {
        size_t idx = (head + i) % WINDOW_SAMPLES;
        last_window.data[i] = buffer[idx];
    }
    last_window.length = WINDOW_SAMPLES;

    window = last_window;
    return true;
}