/*
 * Project: Parkinson's Disease Symptom Monitor (RTES Final Project)
 * Target Hardware: B-L475E-IOT01A (STM32L475)
 * Description: 
 * Real-time monitoring system for detecting Tremor, Dyskinesia, and Freezing of Gait (FOG).
 * Uses raw accelerometer data (LSM6DSL) via bare-metal I2C for stability.
 * Implements FFT-based spectral analysis and a state machine with context awareness.
 * Transmits status and intensity levels via BLE to a mobile application.
 *
 * Key Features:
 * - Bare-metal I2C driver for LSM6DSL (104Hz ODR, 2g FS).
 * - Smart DC Removal for gravity compensation.
 * - Context-aware FOG detection (Walking Memory).
 * - State Hysteresis (Locking mechanism) for FOG stability.
 * - GUI-friendly BLE descriptors.
 *
 * Date: 2025-11-28
 */

#include <Arduino.h>
#include <SPI.h>
#include <Wire.h>
#include <STM32duinoBLE.h>
#include <arduinoFFT.h>

/* -------------------------------------------------------------------------- */
/* Hardware Definitions                         */
/* -------------------------------------------------------------------------- */
// I2C Pin Configurations for B-L475E-IOT01A
#define SDA_PIN PB11
#define SCL_PIN PB10

// LSM6DSL Sensor Registers (Bare Metal Access)
#define LSM6DSL_ADDR      0x6A  // I2C Device Address
#define CTRL1_XL          0x10  // Accelerometer Control Register (ODR & Scale)
#define CTRL3_C           0x12  // Control Register 3 (Reset & Auto-increment)
#define OUTX_L_XL         0x28  // Output Data Register (X-axis Low Byte)

// Direct I2C Interface
TwoWire dev_i2c(SDA_PIN, SCL_PIN);

/**
 * @brief  Writes a byte to a specific sensor register.
 * @param  reg: Register address
 * @param  val: Value to write
 */
void writeRegister(uint8_t reg, uint8_t val) {
  dev_i2c.beginTransmission(LSM6DSL_ADDR);
  dev_i2c.write(reg);
  dev_i2c.write(val);
  dev_i2c.endTransmission();
}

/* -------------------------------------------------------------------------- */
/* BLE Configuration                            */
/* -------------------------------------------------------------------------- */
// Main Service UUID
BLEService pdService("12345678-1234-1234-1234-1234567890AB"); 

// Characteristics Definitions (Read + Notify)
BLEUnsignedCharCharacteristic tremorChar("12345678-1234-1234-1234-123456780001", BLERead | BLENotify);
BLEUnsignedCharCharacteristic dyskinesiaChar("12345678-1234-1234-1234-123456780002", BLERead | BLENotify);
BLEUnsignedCharCharacteristic fogChar("12345678-1234-1234-1234-123456780003", BLERead | BLENotify);

// User Description Descriptors (UUID 0x2901) for GUI labeling
BLEDescriptor tremorDesc("2901", "Tremor (%)");
BLEDescriptor dyskDesc("2901", "Dyskinesia (%)");
BLEDescriptor fogDesc("2901", "FOG (Active?)");

/* -------------------------------------------------------------------------- */
/* DSP & FFT Parameters                         */
/* -------------------------------------------------------------------------- */
#define SAMPLING_FREQ 52      // Target Sampling Rate (Hz)
#define SAMPLES 256           // FFT Size (Must be power of 2)
#define REAL_SAMPLES 156      // Effective Data Points (3 seconds * 52Hz)

// Signal Buffers
double vReal[SAMPLES];        // Real part (Magnitude)
double vImag[SAMPLES];        // Imaginary part (Zeroed)

// Sampling Timing Control
int sampleIndex = 0;          
unsigned long lastSampleTime = 0;
const unsigned long SAMPLE_PERIOD_US = 1000000 / SAMPLING_FREQ; 

// FFT Object Initialization
ArduinoFFT<double> FFT = ArduinoFFT<double>(vReal, vImag, SAMPLES, SAMPLING_FREQ);

// Forward Declaration
void processSignal();

/* -------------------------------------------------------------------------- */
/* Setup Routine                                */
/* -------------------------------------------------------------------------- */
void setup() {
  // 1. Initialize Serial Communication
  Serial.begin(115200);
  pinMode(LED_BUILTIN, OUTPUT);
  delay(2000); // Allow time for Serial Monitor to connect
  Serial.println("\n=== SYSTEM INITIALIZATION ===");

  // 2. Initialize I2C Bus
  dev_i2c.begin();
  
  // 3. Sensor Configuration (LSM6DSL)
  // Enable Block Data Update (BDU) and Address Auto-increment
  writeRegister(CTRL3_C, 0x44); 
  delay(50);
  // Set ODR to 104Hz (High Performance) and Full Scale to 2g
  // Note: We oversample at 104Hz but collect at 52Hz for FFT stability.
  writeRegister(CTRL1_XL, 0x40); 
  delay(100);

  // 4. Initialize BLE Stack
  if (!BLE.begin()) { 
    Serial.println("Error: BLE Init Failed!"); 
    while (1); 
  }
  
  BLE.setLocalName("PD_Monitor");
  BLE.setAdvertisedService(pdService);
  
  // Attach Descriptors to Characteristics
  tremorChar.addDescriptor(tremorDesc);
  pdService.addCharacteristic(tremorChar);
  
  dyskinesiaChar.addDescriptor(dyskDesc);
  pdService.addCharacteristic(dyskinesiaChar);
  
  fogChar.addDescriptor(fogDesc);
  pdService.addCharacteristic(fogChar);

  // Start Advertising
  BLE.addService(pdService);
  BLE.advertise();
  
  Serial.println("System Ready. Advertising...");
}

/* -------------------------------------------------------------------------- */
/* Main Loop                                    */
/* -------------------------------------------------------------------------- */
void loop() {
  // Maintain BLE connectivity
  BLE.poll(); 
  
  unsigned long now = micros();
  
  // Enforce Sampling Rate (52Hz)
  if (now - lastSampleTime >= SAMPLE_PERIOD_US) {
    if (sampleIndex < REAL_SAMPLES) {
      
      // Request 6 bytes of accelerometer data (X, Y, Z)
      dev_i2c.beginTransmission(LSM6DSL_ADDR);
      dev_i2c.write(OUTX_L_XL);
      dev_i2c.endTransmission(false);
      dev_i2c.requestFrom(LSM6DSL_ADDR, 6);

      if (dev_i2c.available() >= 6) {
        lastSampleTime = now;
        
        // Combine High and Low bytes (Little Endian)
        int16_t rawX = dev_i2c.read() | (dev_i2c.read() << 8);
        int16_t rawY = dev_i2c.read() | (dev_i2c.read() << 8);
        int16_t rawZ = dev_i2c.read() | (dev_i2c.read() << 8);
        
        // Convert to mg (Sensitivity for 2g FS is ~0.061 mg/LSB)
        float scale = 0.061;
        float x = rawX * scale;
        float y = rawY * scale;
        float z = rawZ * scale;
        
        // Calculate Euclidean Norm (Magnitude)
        vReal[sampleIndex] = sqrt(x*x + y*y + z*z);
        vImag[sampleIndex] = 0.0;
        
        sampleIndex++;
      }
    }
    else {
      // Buffer full (3 seconds of data), start signal processing
      processSignal(); 
      
      // Reset buffer for next window
      sampleIndex = 0;
      for (int i = 0; i < SAMPLES; i++) { 
        vReal[i] = 0.0; 
        vImag[i] = 0.0; 
      }
    }
  }
}

/* -------------------------------------------------------------------------- */
/* Signal Processing                            */
/* -------------------------------------------------------------------------- */
/**
 * @brief  Performs FFT analysis and executes the State Machine for symptom detection.
 * Logic Flow:
 * 1. DC Removal (Gravity Compensation)
 * 2. FFT Computation
 * 3. Energy Integration (Frequency Bands)
 * 4. Symptom Classification (Priority-based State Machine)
 */
void processSignal() {
  digitalWrite(LED_BUILTIN, HIGH); // Indicator ON

  // --- Step 1: Manual DC Removal (Gravity Compensation) ---
  double meanVal = 0;
  for (int i = 0; i < REAL_SAMPLES; i++) meanVal += vReal[i];
  meanVal /= REAL_SAMPLES;
  
  for (int i = 0; i < SAMPLES; i++) {
    if (i < REAL_SAMPLES) vReal[i] -= meanVal; // Subtract Mean
    else vReal[i] = 0; // Zero-padding
  }

  // --- Step 2: FFT Computation ---
  FFT.windowing(FFT_WIN_TYP_HAMMING, FFT_FORWARD);
  FFT.compute(FFT_FORWARD);
  FFT.complexToMagnitude(); 
  
  // --- Step 3: Spectral Energy Calculation ---
  // Resolution = 52Hz / 256 ≈ 0.203 Hz per bin
  double energyLocomotor = 0; // 0.5 - 3.0 Hz (Walking)
  double energyTremor = 0;    // 3.0 - 5.0 Hz (Resting Tremor)
  double energyDyskinesia = 0;// 5.0 - 7.0 Hz (Dyskinesia)
  double energyFreeze = 0;    // 3.0 - 8.0 Hz (FOG Band)
  
  for (int i = 2; i < (SAMPLES/2); i++) {
    double freq = i * 0.203;
    double val = vReal[i];
    
    if (freq >= 0.5 && freq <= 3.0) energyLocomotor += val;
    if (freq >= 3.0 && freq <= 5.0) energyTremor += val; 
    if (freq >= 5.0 && freq <= 7.0) energyDyskinesia += val;
    if (freq >= 3.0 && freq <= 8.0) energyFreeze += val;
  }
  
  // Debug Output
  Serial.print("E[W/T/D/F]: "); 
  Serial.print((int)energyLocomotor); Serial.print("/");
  Serial.print((int)energyTremor); Serial.print("/");
  Serial.print((int)energyDyskinesia); Serial.print("/");
  Serial.println((int)energyFreeze);

  // --- Step 4: Symptom Classification Logic ---
  
  // State Variables
  static bool isFrozenState = false;  // FOG Hysteresis Lock
  static bool wasWalking = false;     // Context Memory (Walk -> Freeze)

  uint8_t tremorVal = 0;
  uint8_t dyskinesiaVal = 0;
  uint8_t fogVal = 0;
  
  // Thresholds (Calibrated for DC-removed magnitude)
  const double ACTION_THRESHOLD = 15000.0;      
  const double WALK_THRESHOLD = 10000.0;

  // ------------------------------------------------------------
  // Priority 1: FOG State Maintenance (Locking Mechanism)
  // ------------------------------------------------------------
  if (isFrozenState) {
      // If significant high-frequency energy persists, maintain FOG lock.
      // Logic: FOG is hard to exit until movement stops or changes pattern.
      if (energyFreeze > ACTION_THRESHOLD) {
          fogVal = 1;
          Serial.println(">>> EVENT: FOG (Continuing...)");
          wasWalking = false; // Cannot be walking if frozen
          goto sendBLE;
      } else {
          // Energy dropped below threshold -> FOG resolved
          isFrozenState = false;
          Serial.println(">>> Status: FOG Ended.");
      }
  }

  // ------------------------------------------------------------
  // Priority 2: Resting Tremor Detection
  // ------------------------------------------------------------
  // Logic: Energy is dominant in 3-5Hz band and higher than walking/dyskinesia.
  if (energyTremor > ACTION_THRESHOLD && energyTremor > energyLocomotor && energyTremor > energyDyskinesia) {
    tremorVal = (uint8_t)min(energyTremor / 1000.0, 100.0); // Scale to 0-100%
    Serial.println(">>> EVENT: Tremor (Resting)");
    
    // Clear other states
    isFrozenState = false; 
    wasWalking = false; // Tremor interrupts walking memory
    goto sendBLE;
  }
  
  // ------------------------------------------------------------
  // Priority 3: Dyskinesia Detection
  // ------------------------------------------------------------
  // Logic: Energy is dominant in 5-7Hz band (Faster than tremor).
  else if (energyDyskinesia > ACTION_THRESHOLD && energyDyskinesia > energyTremor) {
    dyskinesiaVal = (uint8_t)min(energyDyskinesia / 1000.0, 100.0);
    Serial.println(">>> EVENT: Dyskinesia");
    
    isFrozenState = false;
    wasWalking = false; // Dyskinesia interrupts walking memory
    goto sendBLE;
  }

  // ------------------------------------------------------------
  // Priority 4: FOG Trigger Detection
  // ------------------------------------------------------------
  // Logic covers two scenarios:
  // A. Mixed Mode: Walking energy + High Frequency noise concurrently.
  // B. Context Mode: Walking in previous window + High Frequency noise now.
  else if (
      (energyLocomotor > WALK_THRESHOLD && (energyFreeze / energyLocomotor > 1.5)) ||
      (wasWalking && energyFreeze > ACTION_THRESHOLD && energyFreeze > energyLocomotor)
  ) {
       fogVal = 1;
       isFrozenState = true; // Activate Lock
       wasWalking = false;   // Consume memory
       Serial.println(">>> EVENT: FOG (Started!)");
       goto sendBLE;
  } 

  // ------------------------------------------------------------
  // Priority 5: Normal Walking (Context Update)
  // ------------------------------------------------------------
  // Logic: Dominant low frequency energy (0.5-3Hz).
  else if (energyLocomotor > WALK_THRESHOLD) {
      Serial.println(">>> Status: Walking...");
      wasWalking = true; // Set Context Memory for next window
  } 
  else {
      Serial.println(">>> Status: Idle (Still)");
      wasWalking = false; // Reset memory if idle for too long
  }
  
  // --- Step 5: BLE Transmission ---
  sendBLE:
  if (BLE.connected()) {
    tremorChar.writeValue(tremorVal);
    dyskinesiaChar.writeValue(dyskinesiaVal);
    fogChar.writeValue(fogVal);
  }

  digitalWrite(LED_BUILTIN, LOW); // Indicator OFF
}