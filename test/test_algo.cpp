#include <ArduinoFake.h> 
#include <arduinoFFT.h>
#include <unity.h>       
#include <math.h>

// === 1. Parameters ===
#define SAMPLING_FREQ 52
#define SAMPLES 256
#define REAL_SAMPLES 156

// Thresholds calibrated for mg units (consistent with main.cpp)
// NOISE_THRESHOLD filters out spectral leakage from strong walking signals
// WALKING_THRESHOLD ensures FOG is only calculated during active movement
#define NOISE_THRESHOLD 10000.0      
#define WALKING_THRESHOLD 5000.0   
#define FOG_INDEX_THRESHOLD 2.0

double vReal[SAMPLES];
double vImag[SAMPLES];
ArduinoFFT<double> FFT = ArduinoFFT<double>(vReal, vImag, SAMPLES, SAMPLING_FREQ);

struct DetectionResult {
    uint8_t tremor;
    uint8_t dyskinesia;
    uint8_t fog;
};

// === 2. Signal Generator ===
// Simulates accelerometer data in mg
void generate_signal(double freq1, double amp1, double freq2 = 0, double amp2 = 0) {
    for (int i = 0; i < REAL_SAMPLES; i++) {
        double t = (double)i / SAMPLING_FREQ; 
        double val = amp1 * sin(2 * PI * freq1 * t);
        if (amp2 > 0) {
            val += amp2 * sin(2 * PI * freq2 * t);
        }
        vReal[i] = val; 
        vImag[i] = 0.0;
    }
    for (int i = REAL_SAMPLES; i < SAMPLES; i++) {
        vReal[i] = 0.0; vImag[i] = 0.0;
    }
}

// === 3. Core Algorithm (Mirror of main.cpp) ===
DetectionResult run_algorithm() {
    FFT.dcRemoval();
    FFT.windowing(FFT_WIN_TYP_HAMMING, FFT_FORWARD);
    FFT.compute(FFT_FORWARD);
    FFT.complexToMagnitude();

    double energyLocomotor = 0; 
    double energyTremor = 0;   
    double energyDyskinesia = 0;
    double energyFreeze = 0;    
    
    for (int i = 2; i < (SAMPLES/2); i++) {
        double freq = i * 0.203;
        double val = vReal[i];
        
        if (freq >= 0.5 && freq <= 3.0) energyLocomotor += val;
        if (freq >= 3.0 && freq <= 5.0) energyTremor += val;
        if (freq >= 5.0 && freq <= 7.0) energyDyskinesia += val;
        if (freq >= 3.0 && freq <= 8.0) energyFreeze += val;
    }

    printf("  >> Energies -> Walk: %.0f | Tremor: %.0f | Dysk: %.0f | Freeze: %.0f\n", 
           energyLocomotor, energyTremor, energyDyskinesia, energyFreeze);

    DetectionResult res = {0, 0, 0};

    // Detection Logic
    if (energyTremor > NOISE_THRESHOLD && energyTremor > energyDyskinesia) {
        res.tremor = 1;
    }
    
    if (energyDyskinesia > NOISE_THRESHOLD && energyDyskinesia > energyTremor) {
        res.dyskinesia = 1;
    }

    if (energyLocomotor > WALKING_THRESHOLD) {
         double freezeIndex = energyFreeze / energyLocomotor;
         printf("  >> Freeze Index: %.2f\n", freezeIndex);
         if (freezeIndex > FOG_INDEX_THRESHOLD) {
           res.fog = 1;
         }
    }

    return res;
}

// === 4. Test Cases ===

// Test 1: Idle State
// Simulates sensor noise (20mg). Should detect nothing.
void test_idle_noise(void) {
    printf("\n[Test 1] Real Sensor Noise (Amp=20mg)\n");
    generate_signal(1.0, 20.0); 
    DetectionResult res = run_algorithm();
    TEST_ASSERT_EQUAL(0, res.tremor);
    TEST_ASSERT_EQUAL(0, res.fog);
}

// Test 2: Tremor Event
// Simulates typical resting tremor (4Hz, 300mg)
void test_tremor_event(void) {
    printf("\n[Test 2] Typical Tremor (4Hz, Amp=300mg)\n");
    generate_signal(4.0, 300.0); 
    DetectionResult res = run_algorithm();
    TEST_ASSERT_EQUAL(1, res.tremor);
}

// Test 3: Dyskinesia Event
// Simulates bradykinesia/dyskinesia (6Hz, 300mg)
void test_dyskinesia_event(void) {
    printf("\n[Test 3] Typical Dyskinesia (6Hz, Amp=300mg)\n");
    generate_signal(6.0, 300.0); 
    DetectionResult res = run_algorithm();
    TEST_ASSERT_EQUAL(1, res.dyskinesia);
}

// Test 4: Normal Walking
// Simulates arm swing (2Hz, 400mg). 
// Verifies that high thresholds prevent spectral leakage from triggering false tremor.
void test_normal_walking(void) {
    printf("\n[Test 4] Normal Walking (2Hz, Amp=400mg)\n");
    generate_signal(2.0, 400.0); 
    DetectionResult res = run_algorithm();
    TEST_ASSERT_EQUAL(0, res.tremor); 
    TEST_ASSERT_EQUAL(0, res.fog);
}

// Test 5: FOG Event
// Scenario: Patient trying to walk (weak low freq) + High freq trembling.
// FOG triggered when Freeze Energy > 2x Locomotor Energy.
void test_fog_event(void) {
    printf("\n[Test 5] FOG Event: Weak Walk (100mg) + Moderate Freeze (300mg)\n");
    generate_signal(2.0, 100.0, 5.0, 300.0); 
    DetectionResult res = run_algorithm();
    TEST_ASSERT_EQUAL(1, res.fog);
}

// Test 6: Resting False Positive Check
// Simulates slight shake while stationary.
// Should NOT trigger FOG because Locomotor Energy < WALKING_THRESHOLD.
void test_resting_false_positive(void) {
    printf("\n[Test 6] Resting with slight shake (50mg)\n");
    generate_signal(5.0, 50.0); 
    DetectionResult res = run_algorithm();
    TEST_ASSERT_EQUAL(0, res.fog); 
}

int main(int argc, char **argv) {
    UNITY_BEGIN();
    RUN_TEST(test_idle_noise);
    RUN_TEST(test_tremor_event);
    RUN_TEST(test_dyskinesia_event);
    RUN_TEST(test_normal_walking);
    RUN_TEST(test_fog_event);
    RUN_TEST(test_resting_false_positive);
    UNITY_END();
    return 0;
}