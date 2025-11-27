# GaitWave — RTES Parkinson’s Detection Project

GaitWave is an embedded real-time system built on the **ST B-L475E-IOT01A (STM32L475)** board.  
The device detects three Parkinson’s-related motion conditions using onboard sensors:

- **Tremor detection** (3–5 Hz)
- **Dyskinesia detection** (5–7 Hz)
- **Freezing of Gait (FOG)** using step detection and cadence analysis

All processing is done on-device using IMU sampling, a 3-second sliding window, FFT, and gait analysis.  
Results are transmitted wirelessly via **Bluetooth Low Energy (BLE)**.

---

## Features
- IMU sampling at 52 Hz (accelerometer + gyroscope)
- 3-second double-buffered data window
- FFT-based frequency analysis for tremor/dyskinesia
- Step and cadence analysis for FOG detection
- BLE service with three characteristics:
    - Tremor level
    - Dyskinesia level
    - FOG state
- LED and button feedback for user interaction

---

## Requirements
- ST B-L475E-IOT01A Discovery Kit
- PlatformIO (Mbed framework)
- CLion IDE / VSCode (optional)
- Mobile BLE scanner

---

## Team
**GaitWave Team — Fall 2025 NYU**