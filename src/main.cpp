/*
 * Project: Parkinson's Disease Symptom Monitor (RTES Final Project)
 * Target Hardware: B-L475E-IOT01A (STM32L475)
 * Version: 4.1 (Stable Core + GUI Enhanced)
 * Date: 2025-11-28
 *
 * Description: 
 * Real-time embedded system for detecting Parkinsonian motor symptoms:
 * 1. Resting Tremor (3-5 Hz)
 * 2. Dyskinesia (5-7 Hz, High Intensity)
 * 3. Freezing of Gait (FOG) (3-8 Hz, Context-Aware)
 *
 * Technical Implementation:
 * - Sensor: LSM6DSL via Bare-Metal I2C (Direct Register Access).
 * - Signal Processing: 256-point FFT with Hamming Window.
 * - Logic: Priority-based State Machine with Hysteresis and Walking Context Memory.
 * - Connectivity: BLE Notification with User Description Descriptors (0x2901).
 */

#include <Arduino.h>
#include <SPI.h>
#include <Wire.h>
#include <STM32duinoBLE.h>
#include <arduinoFFT.h>

/* -------------------------------------------------------------------------- */
/* Section 1: Hardware & Driver Definitions                                   */
/* -------------------------------------------------------------------------- */

// I2C Pin Configurations
#define SDA_PIN PB11
#define SCL_PIN PB10

// LSM6DSL Register Addresses (Bare Metal Access)
#define LSM6DSL_ADDR      0x6A  // Device Address
#define CTRL1_XL          0x10  // Accelerometer Control (ODR/Scale)
#define CTRL3_C           0x12  // Control Register 3 (Reset/Inc)
#define OUTX_L_XL         0x28  // Output Data Start Address

// Global I2C Interface
TwoWire dev_i2c(SDA_PIN, SCL_PIN);

/**
 * @brief  Low-level function to write to sensor registers.
 * @param  reg: Target register address
 * @param  val: Byte value to write
 */
void writeRegister(uint8_t reg, uint8_t val) {
  dev_i2c.beginTransmission(LSM6DSL_ADDR);
  dev_i2c.write(reg);
  dev_i2c.write(val);
  dev_i2c.endTransmission();
}

/* -------------------------------------------------------------------------- */
/* Section 2: BLE Service & Characteristics                                   */
/* -------------------------------------------------------------------------- */

// Main Service UUID
BLEService pdService("12345678-1234-1234-1234-1234567890AB"); 

// Characteristics for Symptom Data
BLEUnsignedCharCharacteristic tremorChar("12345678-1234-1234-1234-123456780001", BLERead | BLENotify);
BLEUnsignedCharCharacteristic dyskinesiaChar("12345678-1234-1234-1234-123456780002", BLERead | BLENotify);
BLEUnsignedCharCharacteristic fogChar("12345678-1234-1234-1234-123456780003", BLERead | BLENotify);

// User Description Descriptors (UUID 0x2901)
// Used to provide human-readable labels in the BLE App
BLEDescriptor tremorDesc("2901", "Tremor (%)");
BLEDescriptor dyskDesc("2901", "Dyskinesia (%)");
BLEDescriptor fogDesc("2901", "FOG (Active?)");

/* -------------------------------------------------------------------------- */
/* Section 3: DSP & Sampling Parameters                                       */
/* -------------------------------------------------------------------------- */

#define SAMPLING_FREQ 52      // Hz
#define SAMPLES 256           // Points (Power of 2 for FFT)
#define REAL_SAMPLES 156      // Actual Data Points (3 seconds window)

// FFT Data Buffers
double vReal[SAMPLES];
double vImag[SAMPLES];

// Timing Control
int sampleIndex = 0;          
unsigned long lastSampleTime = 0;
const unsigned long SAMPLE_PERIOD_US = 1000000 / SAMPLING_FREQ; 

// FFT Instance
ArduinoFFT<double> FFT = ArduinoFFT<double>(vReal, vImag, SAMPLES, SAMPLING_FREQ);

// Function Prototype
void processSignal();

/* -------------------------------------------------------------------------- */
/* Section 4: System Initialization                                           */
/* -------------------------------------------------------------------------- */
void setup() {
  // 1. Serial Init
  Serial.begin(115200);
  pinMode(LED_BUILTIN, OUTPUT);
  delay(2000);
  Serial.println("\n=== SYSTEM V4.1 (GUI ENHANCED) ===");

  // 2. Hardware Driver Init
  dev_i2c.begin();
  // Configure Sensor: BDU enabled, Address Auto-Inc enabled
  writeRegister(CTRL3_C, 0x44); 
  delay(50);
  // Configure Sensor: 104Hz ODR, 2g Full Scale (High Performance Mode)
  writeRegister(CTRL1_XL, 0x40); 
  delay(100);

  // 3. BLE Stack Init
  if (!BLE.begin()) { 
    Serial.println("Error: BLE Init Failed!"); 
    while (1); 
  }
  
  BLE.setLocalName("PD_Monitor");
  BLE.setAdvertisedService(pdService);
  
  // Attach Descriptors for GUI
  tremorChar.addDescriptor(tremorDesc);
  pdService.addCharacteristic(tremorChar);
  
  dyskinesiaChar.addDescriptor(dyskDesc);
  pdService.addCharacteristic(dyskinesiaChar);
  
  fogChar.addDescriptor(fogDesc);
  pdService.addCharacteristic(fogChar);

  // Start Broadcasting
  BLE.addService(pdService);
  BLE.advertise();
  
  Serial.println("System Ready. Check App for Labels.");
}

/* -------------------------------------------------------------------------- */
/* Section 5: Main Loop (Data Acquisition)                                    */
/* -------------------------------------------------------------------------- */
void loop() {
  BLE.poll(); // Maintain connection
  unsigned long now = micros();
  
  // Precise Sampling Timing Enforcement
  if (now - lastSampleTime >= SAMPLE_PERIOD_US) {
    if (sampleIndex < REAL_SAMPLES) {
      
      // Read 6 Bytes from Sensor
      dev_i2c.beginTransmission(LSM6DSL_ADDR);
      dev_i2c.write(OUTX_L_XL);
      dev_i2c.endTransmission(false);
      dev_i2c.requestFrom(LSM6DSL_ADDR, 6);

      if (dev_i2c.available() >= 6) {
        lastSampleTime = now;
        
        // Data Reconstruction (Little Endian)
        int16_t rawX = dev_i2c.read() | (dev_i2c.read() << 8);
        int16_t rawY = dev_i2c.read() | (dev_i2c.read() << 8);
        int16_t rawZ = dev_i2c.read() | (dev_i2c.read() << 8);
        
        // Unit Conversion to mg
        float scale = 0.061;
        float x = rawX * scale;
        float y = rawY * scale;
        float z = rawZ * scale;
        
        // Calculate Signal Magnitude Vector (SMV)
        vReal[sampleIndex] = sqrt(x*x + y*y + z*z);
        vImag[sampleIndex] = 0.0;
        
        sampleIndex++;
      }
    }
    else {
      // Buffer Full -> Trigger Signal Processing
      processSignal(); 
      
      // Reset Buffer
      sampleIndex = 0;
      for (int i = 0; i < SAMPLES; i++) { vReal[i] = 0.0; vImag[i] = 0.0; }
    }
  }
}

/* -------------------------------------------------------------------------- */
/* Section 6: Signal Processing & State Machine                               */
/* -------------------------------------------------------------------------- */
/**
 * @brief  Analyzes the accelerometer data window and updates symptom status.
 * Implements a priority-based state machine.
 */
void processSignal() {
  digitalWrite(LED_BUILTIN, HIGH);

  // --- Step 1: Manual DC Removal (Gravity Compensation) ---
  // Calculates mean value and subtracts it to remove the 0Hz component.
  double meanVal = 0;
  for (int i = 0; i < REAL_SAMPLES; i++) meanVal += vReal[i];
  meanVal /= REAL_SAMPLES;
  for (int i = 0; i < SAMPLES; i++) {
    if (i < REAL_SAMPLES) vReal[i] -= meanVal;
    else vReal[i] = 0;
  }

  // --- Step 2: Fast Fourier Transform (FFT) ---
  FFT.windowing(FFT_WIN_TYP_HAMMING, FFT_FORWARD);
  FFT.compute(FFT_FORWARD);
  FFT.complexToMagnitude(); 
  
  // --- Step 3: Energy Integration per Frequency Band ---
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
  
  // Debug Log
  Serial.print("E[W/T/D/F]: "); 
  Serial.print((int)energyLocomotor); Serial.print("/");
  Serial.print((int)energyTremor); Serial.print("/");
  Serial.print((int)energyDyskinesia); Serial.print("/");
  Serial.println((int)energyFreeze);

  // =========================================================
  // Core Logic: Symptom Classification State Machine
  // =========================================================
  
  static bool isFrozenState = false;  // FOG Hysteresis Lock
  static bool wasWalking = false;     // Walking Context Memory

  uint8_t tremorVal = 0;
  uint8_t dyskinesiaVal = 0;
  uint8_t fogVal = 0;
  
  const double ACTION_THRESHOLD = 15000.0;      
  const double WALK_THRESHOLD = 10000.0;

  // ------------------------------------------------------------
  // Priority 1: FOG Maintenance & Smart Release
  // ------------------------------------------------------------
  // If system is currently in FOG state, check if we should stay or release.
  if (isFrozenState) {
      // Release Condition: Signal transitions to clear Tremor
      // (Tremor energy dominant over Dyskinesia & Locomotor)
      bool isNowTremor = (energyTremor > ACTION_THRESHOLD) && 
                         (energyTremor > energyDyskinesia) && 
                         (energyTremor > energyLocomotor);
      
      if (isNowTremor) {
          isFrozenState = false;
          wasWalking = false;
          tremorVal = (uint8_t)min(energyTremor / 1000.0, 100.0);
          Serial.println(">>> Status: FOG -> Tremor Transition.");
          goto sendBLE; // Exit to BLE update
      }
      
      // Maintenance Condition: High frequency energy persists
      if (energyFreeze > ACTION_THRESHOLD) {
          fogVal = 1;
          Serial.println(">>> EVENT: FOG (Continuing...)");
          wasWalking = false; 
          goto sendBLE;
      } else {
          // Exit Condition: Energy drops (Return to Idle)
          isFrozenState = false;
          Serial.println(">>> Status: FOG Ended (Idle).");
      }
  }

  // ------------------------------------------------------------
  // Priority 2: Resting Tremor Detection
  // ------------------------------------------------------------
  // Detected if Tremor band is dominant.
  if (energyTremor > ACTION_THRESHOLD && energyTremor > energyLocomotor && energyTremor > energyDyskinesia) {
    tremorVal = (uint8_t)min(energyTremor / 1000.0, 100.0);
    Serial.println(">>> EVENT: Tremor (Resting)");
    
    isFrozenState = false; 
    wasWalking = false; // Tremor interrupts walking context
    goto sendBLE;
  }
  
  // ------------------------------------------------------------
  // Priority 3: Dyskinesia Detection
  // ------------------------------------------------------------
  // Detected if Dyskinesia band is dominant (Higher freq, chaotic).
  else if (energyDyskinesia > ACTION_THRESHOLD && energyDyskinesia > energyTremor) {
    dyskinesiaVal = (uint8_t)min(energyDyskinesia / 1000.0, 100.0);
    Serial.println(">>> EVENT: Dyskinesia");
    
    isFrozenState = false;
    wasWalking = false; // Dyskinesia interrupts walking context
    goto sendBLE;
  }

  // ------------------------------------------------------------
  // Priority 4: FOG Trigger Detection (Entry Conditions)
  // ------------------------------------------------------------
  else if (
      // Scenario A: Mixed Mode (Walking & Freezing occur in same window)
      (energyLocomotor > WALK_THRESHOLD && (energyFreeze / energyLocomotor > 1.5))
      ||
      // Scenario B: Context Mode (Walking in previous window -> High Freq now)
      (wasWalking && energyFreeze > ACTION_THRESHOLD && energyFreeze > energyLocomotor)
  ) {
       fogVal = 1;
       isFrozenState = true; // Engage FOG Lock
       wasWalking = false;   // Consume Walking Memory
       Serial.println(">>> EVENT: FOG (Started!)");
       goto sendBLE;
  } 

  // ------------------------------------------------------------
  // Priority 5: Walking Context Update
  // ------------------------------------------------------------
  else if (energyLocomotor > WALK_THRESHOLD) {
      Serial.println(">>> Status: Walking...");
      wasWalking = true; // Set Context for next window
  } 
  else {
      Serial.println(">>> Status: Idle (Still)");
      wasWalking = false; // Reset Context
  }
  
  // --- BLE Transmission ---
  sendBLE:
  if (BLE.connected()) {
    tremorChar.writeValue(tremorVal);
    dyskinesiaChar.writeValue(dyskinesiaVal);
    fogChar.writeValue(fogVal);
  }

  digitalWrite(LED_BUILTIN, LOW);
}