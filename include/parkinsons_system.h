#ifndef PARKINSONS_SYSTEM_H
#define PARKINSONS_SYSTEM_H

#include "mbed.h"
#include <cstdint>

// ===================================================
// Parkinson's Disease Symptom Detection System
// GaitWave - Real-time Embedded Detection Platform
// ===================================================

// === Sensor Configuration ===
#define LSM6DSL_ADDR        (0x6A << 1)
#define WHO_AM_I            0x0F
#define CTRL1_XL            0x10
#define CTRL2_G             0x11
#define CTRL3_C             0x12
#define OUTX_L_XL           0x28
#define OUTY_L_XL           0x2A
#define OUTZ_L_XL           0x2C

// === Sampling and FFT Parameters ===
#define SAMPLE_RATE         52.0f        // Hz
#define SAMPLE_PERIOD_MS    19           // ms
#define BUFFER_SIZE         156          // 3 seconds * 52Hz
#define GAIT_FFT_SIZE       256          // Power of 2, zero-padded

// === Detection Frequency Bands ===
#define TREMOR_LOW_HZ       3.0f
#define TREMOR_HIGH_HZ      5.0f
#define DYSKINESIA_LOW_HZ   5.0f
#define DYSKINESIA_HIGH_HZ  7.0f

// === Sensor Data Structure ===
struct SensorData {
    float accel_x[BUFFER_SIZE];
    float accel_y[BUFFER_SIZE];
    float accel_z[BUFFER_SIZE];
    uint16_t index;
};

// === Detection Results Structure ===
struct DetectionResults {
    bool tremor_detected;
    float tremor_intensity;      // 0-100%
    bool dyskinesia_detected;
    float dyskinesia_intensity;  // 0-100%
    bool freezing_detected;
    float freezing_confidence;   // 0-100%
};

// ===================================================
// External Hardware Declarations
// ===================================================
extern I2C i2c;
extern BufferedSerial serial_port;
extern DigitalOut led1;
extern DigitalOut led2;
extern DigitalOut led3;
extern InterruptIn button;

// === External Global Data ===
extern SensorData sensor_data;
extern DetectionResults results;
extern bool sensor_initialized;
extern bool button_pressed;

// ===================================================
// Math Functions
// ===================================================
float sqrt_custom(float x);
float cos_custom(float angle);
float sin_custom(float angle);
int log2_custom(int n);

// ===================================================
// I2C Communication
// ===================================================
void write_register(uint8_t reg, uint8_t value);
bool read_register(uint8_t reg, uint8_t &value);
int16_t read_int16(uint8_t reg_low);

// ===================================================
// Sensor Initialization and Data Collection
// ===================================================
bool initialize_sensor();
void read_accelerometer(float &acc_x, float &acc_y, float &acc_z);
void collect_data_sample(float acc_x, float acc_y, float acc_z);
bool buffer_is_full();

// ===================================================
// FFT and Frequency Analysis
// ===================================================
void fft_complex(float *real, float *imag, int n);
float analyze_frequency_band(float *data, float freq_low, float freq_high);

// ===================================================
// Symptom Detection
// ===================================================
void detect_symptoms();
void transmit_results();
void on_button_press();

#endif // PARKINSONS_SYSTEM_H
