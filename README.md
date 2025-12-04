


# **GaitMate - RTES Parkinson’s Detection Project**

GaitMate is a real-time embedded system built on the **ST B-L475E-IOT01A (STM32L475)** Discovery board.  
The device detects key Parkinson’s-related movement conditions using onboard IMU sensors and DSP analysis:

- **Tremor detection** (3–7 Hz bandpower)
- **Dyskinesia detection** (0.5–3 Hz bandpower)
- **Freezing of Gait (FOG)** using step-rate + variability analysis

All computation runs **fully on-device** using a high-frequency IMU stream, a sliding window, FFT processing, gait features, and a BLE notification pipeline.  
Process results are broadcast wirelessly using **Bluetooth Low Energy (BLE)**.

---

## 🚀 Features

- IMU sampling at **~104 Hz**
- Real-time **1.5–2s sliding window** with continuous processing
- **CMSIS-DSP FFT** for tremor & dyskinesia frequency band analysis
- **Step detection** and cadence estimation
- **Variability score** for movement irregularity
- **FOG detection** triggered when cadence + variance meet threshold
- BLE service streaming:
  - Tremor level
  - Dyskinesia level
  - Steps per minute
  - FOG state
  - Variability score
- LED/button support for debugging and demo interactions
- Deterministic real-time execution with no missed deadlines

---

## 📡 BLE Interface

GaitMate exposes a custom BLE service with automatic notifications.  
Example message:

You can read the data using:

- **LightBlue (iOS / Android)**
- **nRF Connect**
- Any BLE scanner app

---

## Output Interpretation

| Field      | Meaning |
|-----------|----------|
| **tremor** | Tremor severity (0–100) |
| **dysk** | Dyskinesia severity (0–100) |
| **steps** | Step rate (steps per minute) |
| **fog** | 1 = freezing-of-gait, 0 = normal |
| **var** | Movement variability (instability metric) |

---

## 🛠 Requirements

- **ST B-L475E-IOT01A Discovery Kit**
- PlatformIO (Mbed framework)
- CLion or VSCode (optional)
- Mobile BLE scanner (LightBlue recommended)

---

## 🏗 Build & Upload

```bash
pio run
pio run -t upload
pio device monitor -b 115200