#include "ble_service.h"
#include "gait.h"
#include "math.h"
#include "parkinsons_system.h"

// ===================================================
// Hardware Initialization
// ===================================================
I2C i2c(PB_11, PB_10);
BufferedSerial serial_port(USBTX, USBRX, 115200);
FileHandle *mbed::mbed_override_console(int) { return &serial_port; }

// LED Indicators
DigitalOut led1(LED1);  // LD1 - Green - Tremor
DigitalOut led2(LED2);  // LD2 - Green - Dyskinesia
DigitalOut led3(LED3);  // LD3 - Yellow - Freezing
InterruptIn button(BUTTON1);

// === Global Variables ===
SensorData sensor_data = {{0}, {0}, {0}, {0}, 0};
DetectionResults results = {false, 0, false, 0, false, 0};
bool sensor_initialized = false;
bool button_pressed = false;

// ===================================================
// I2C Communication
// ===================================================
void write_register(uint8_t reg, uint8_t value) {
  char data[2] = {(char)reg, (char)value};
  i2c.write(LSM6DSL_ADDR, data, 2);
}

bool read_register(uint8_t reg, uint8_t &value) {
  char r = (char)reg;
  if (i2c.write(LSM6DSL_ADDR, &r, 1, true) != 0) return false;
  ThisThread::sleep_for(1ms);
  if (i2c.read(LSM6DSL_ADDR, &r, 1) != 0) return false;
  value = (uint8_t)r;
  return true;
}

int16_t read_int16(uint8_t reg_low) {
  uint8_t low_byte, high_byte;
  if (!read_register(reg_low, low_byte)) return 0;
  if (!read_register(reg_low + 1, high_byte)) return 0;
  return (int16_t)((high_byte << 8) | low_byte);
}

// ===================================================
// Sensor Initialization
// ===================================================
bool initialize_sensor() {
  uint8_t device_id;

  if (!read_register(WHO_AM_I, device_id) || device_id != 0x6A) {
    printf("ERROR: Sensor not found! ID = 0x%02X\r\n", device_id);
    return false;
  }

  printf("Sensor detected: LSM6DSL (ID: 0x%02X)\r\n", device_id);

  write_register(CTRL3_C, 0x44);
  write_register(CTRL1_XL, 0x30);
  write_register(CTRL2_G, 0x00);

  ThisThread::sleep_for(100ms);

  printf("Sensor initialized successfully\r\n");
  fflush(stdout);
  return true;
}

// ===================================================
// Data Collection
// ===================================================
void read_accelerometer(float &acc_x, float &acc_y, float &acc_z) {
  int16_t raw_x = read_int16(OUTX_L_XL);
  int16_t raw_y = read_int16(OUTY_L_XL);
  int16_t raw_z = read_int16(OUTZ_L_XL);

  const float SENSITIVITY = 0.061f / 1000.0f;
  acc_x = raw_x * SENSITIVITY;
  acc_y = raw_y * SENSITIVITY;
  acc_z = raw_z * SENSITIVITY;
}

void collect_data_sample(float acc_x, float acc_y, float acc_z) {
  uint16_t idx = sensor_data.index;
  sensor_data.accel_x[idx] = acc_x;
  sensor_data.accel_y[idx] = acc_y;
  sensor_data.accel_z[idx] = acc_z;
  sensor_data.accel_total[idx] =
      sqrtf(acc_x * acc_x + acc_y * acc_y + acc_z * acc_z);
  sensor_data.index = (idx + 1) % BUFFER_SIZE;
}

bool buffer_is_full() { return sensor_data.index == 0; }

// ===================================================
// FFT Implementation (Cooley-Tukey)
// ===================================================
void fft_complex(float *real, float *imag, int n) {
  if (n <= 1) return;

  for (int i = 1, j = 0; i < n; i++) {
    int bit = n >> 1;
    for (; j & bit; bit >>= 1) {
      j ^= bit;
    }
    j ^= bit;

    if (i < j) {
      float temp_r = real[i];
      float temp_i = imag[i];
      real[i] = real[j];
      imag[i] = imag[j];
      real[j] = temp_r;
      imag[j] = temp_i;
    }
  }

  const float PI = 3.14159265359f;
  for (int s = 1; s <= (int)log2f(n); s++) {
    int m = 1 << s;
    float angle = -2.0f * PI / m;
    float wm_real = cosf(angle);
    float wm_imag = sinf(angle);

    for (int k = 0; k < n; k += m) {
      float w_real = 1.0f;
      float w_imag = 0.0f;

      for (int j = 0; j < m / 2; j++) {
        int t = k + j;
        int u = k + j + m / 2;

        float t_real = real[u] * w_real - imag[u] * w_imag;
        float t_imag = real[u] * w_imag + imag[u] * w_real;

        real[u] = real[t] - t_real;
        imag[u] = imag[t] - t_imag;
        real[t] = real[t] + t_real;
        imag[t] = imag[t] + t_imag;

        float temp_real = w_real * wm_real - w_imag * wm_imag;
        float temp_imag = w_real * wm_imag + w_imag * wm_real;
        w_real = temp_real;
        w_imag = temp_imag;
      }
    }
  }
}

// ===================================================
// FFT and Frequency Analysis
// ===================================================
float analyze_frequency_band(float *data, float freq_low, float freq_high) {
  float fft_real[GAIT_FFT_SIZE];
  float fft_imag[GAIT_FFT_SIZE];

  for (int i = 0; i < GAIT_FFT_SIZE; i++) {
    fft_real[i] = 0;
    fft_imag[i] = 0;
  }

  const float PI = 3.14159265359f;
  for (int i = 0; i < BUFFER_SIZE; i++) {
    if (i < GAIT_FFT_SIZE) {
      float window = 0.5f * (1.0f - cosf(2.0f * PI * i / (BUFFER_SIZE - 1)));
      fft_real[i] = data[i] * window;
    }
  }

  fft_complex(fft_real, fft_imag, GAIT_FFT_SIZE);

  float band_energy = 0.0f;
  float total_energy = 0.0f;

  int bin_low = (int)(freq_low * GAIT_FFT_SIZE / SAMPLE_RATE);
  int bin_high = (int)(freq_high * GAIT_FFT_SIZE / SAMPLE_RATE);

  for (int i = 0; i < GAIT_FFT_SIZE / 2; i++) {
    float magnitude =
        sqrtf(fft_real[i] * fft_real[i] + fft_imag[i] * fft_imag[i]);
    float energy = magnitude * magnitude;
    total_energy += energy;

    if (i >= bin_low && i <= bin_high) {
      band_energy += energy;
    }
  }

  if (total_energy == 0) return 0.0f;
  return (band_energy / total_energy) * 100.0f;
}

// ===================================================
// Detection Algorithm
// ===================================================
void detect_symptoms() {
  if (!buffer_is_full()) return;

  // TODO: change from only considering accel_z to 3 dimensional vector sum
  float tremor_x = analyze_frequency_band(sensor_data.accel_x, TREMOR_LOW_HZ,
                                          TREMOR_HIGH_HZ);
  float tremor_y = analyze_frequency_band(sensor_data.accel_y, TREMOR_LOW_HZ,
                                          TREMOR_HIGH_HZ);
  float tremor_z = analyze_frequency_band(sensor_data.accel_z, TREMOR_LOW_HZ,
                                          TREMOR_HIGH_HZ);
  results.tremor_intensity =
      sqrtf((tremor_x * tremor_x + tremor_y * tremor_y + tremor_z * tremor_z) / 3.0f); // dividing 3 to normalize back to 0-100 scale
  results.tremor_detected = (results.tremor_intensity > 20.0f);

    float dyskinesia_x = analyze_frequency_band(
        sensor_data.accel_x, DYSKINESIA_LOW_HZ, DYSKINESIA_HIGH_HZ);
    float dyskinesia_y = analyze_frequency_band(
        sensor_data.accel_y, DYSKINESIA_LOW_HZ, DYSKINESIA_HIGH_HZ);
    float dyskinesia_z = analyze_frequency_band(
        sensor_data.accel_z, DYSKINESIA_LOW_HZ, DYSKINESIA_HIGH_HZ);
  results.dyskinesia_intensity = sqrtf((dyskinesia_x * dyskinesia_x + dyskinesia_y * dyskinesia_y + dyskinesia_z * dyskinesia_z) / 3.0f); // dividing 3 to normalize back to 0-100 scale
  results.dyskinesia_detected = (results.dyskinesia_intensity > 20.0f);

  // Use the new gait detection from gait.cpp
  GaitStatus gait_status = gait_update(SignalWindow{});
  results.freezing_detected =
      (gait_status.fog_state > 0);  // 1 = freeze start, 2 = sustained
  results.freezing_confidence = (gait_status.fog_state > 0) ? 100.0f : 0.0f; // Simplified confidence (droping the 1 = freeze start, 2 = sustained information)

  // Compact status format: [Tremor|Dyskinesia|Freezing]
  printf("[%s|%s|%s]\r\n", results.tremor_detected ? "T" : " ",
         results.dyskinesia_detected ? "D" : " ",
         results.freezing_detected ? "F" : " ");

  // DEBUG: Print intensities
  printf(
      " Intensities: Tremor: %.1f%% | Dyskinesia: %.1f%% | Freezing: "
      "%.1f%%\r\n",
      results.tremor_intensity, results.dyskinesia_intensity,
      results.freezing_confidence);

  fflush(stdout);
}

// ===================================================
// BLE Service
// ===================================================
void transmit_results() {
  // Empty - LED status handled separately
}

// ===================================================
// Button Handler
// ===================================================
void on_button_press() { button_pressed = true; }

// ===================================================
// Main Program
// ===================================================
int main() {
  printf("\r\n==========================================\r\n");
  printf("  Parkinson's Symptom Detection System   \r\n");
  printf("  GaitWave - Real-time Detection        \r\n");
  printf("==========================================\r\n\r\n");

  button.fall(&on_button_press);
  i2c.frequency(400000);

  sensor_initialized = initialize_sensor();

  if (!sensor_initialized) {
    printf("FATAL: Sensor initialization failed!\r\n");
    fflush(stdout);
    while (1) {
      led1 = !led1;
      ThisThread::sleep_for(200ms);
    }
  }

  gait_init();

  printf("\r\nStarting data acquisition...\r\n");
  printf("Sample Rate: %.0f Hz | Buffer: %d samples (3 sec)\r\n", SAMPLE_RATE,
         BUFFER_SIZE);
  printf("Collecting data, detection begins when buffer fills...\r\n\r\n");
  fflush(stdout);

  int sample_count = 0;

  while (true) {
    float acc_x, acc_y, acc_z;
    read_accelerometer(acc_x, acc_y, acc_z);
    collect_data_sample(acc_x, acc_y, acc_z);

    if (sample_count % 52 == 0) {
      printf("Sample %d | X: %.3f | Y: %.3f | Z: %.3f g\r\n", sample_count,
             acc_x, acc_y, acc_z);
      fflush(stdout);
    }

    if (buffer_is_full()) {
      detect_symptoms();
      transmit_results();
      printf("---\r\n");
      fflush(stdout);
    }

    // Update LEDs: Keep one LED ON (solid) per detected symptom
    // LED1 = Tremor, LED2 = Dyskinesia, LED3 = Freezing
    led1 = results.tremor_detected ? 1 : 0;
    led2 = results.dyskinesia_detected ? 1 : 0;
    led3 = results.freezing_detected ? 1 : 0;

    if (button_pressed) {
      button_pressed = false;
      if (buffer_is_full()) {
        printf("\nManual detection triggered\r\n");
        detect_symptoms();
        transmit_results();
      } else {
        printf("Buffer not ready yet (%d/%d samples)\r\n", sensor_data.index,
               BUFFER_SIZE);
      }
      fflush(stdout);
    }

    sample_count++;
    ThisThread::sleep_for(chrono::milliseconds(SAMPLE_PERIOD_MS));
  }
}
