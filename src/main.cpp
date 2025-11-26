#include "mbed.h"
#include "config.h"
#include "sensors.h"
#include "dsp.h"
#include "gait.h"
#include "ble_service.h"

DigitalOut led(LED1);   // status LED

int main() {
    led = 0;

    bool ok = true;
    ok &= sensors_init();
    dsp_init();
    gait_init();
    ok &= ble_service_init();

    if (!ok) {
        // Fatal error: blink fast
        while (true) {
            led = !led;
            ThisThread::sleep_for(100ms);
        }
    }

    sensors_start();

    while (true) {
        SignalWindow window;
        if (sensors_get_window(window)) {
            // 1. Analyze tremor/dysk
            MovementAnalysis m = dsp_analyze_window(window);

            // 2. Analyze gait / FOG
            GaitStatus g = gait_update(window);

            // 3. Update BLE characteristics
            ble_service_update(m, g);

            // 4. Simple visual feedback: LED on if any issue detected
            bool issue = (m.tremor_level > 0) ||
                         (m.dyskinesia_level > 0) ||
                         (g.fog_state != 0);
            led = issue ? 1 : 0;
        }

        // Let other ISRs run; don't block
        ThisThread::sleep_for(20ms);
    }
}