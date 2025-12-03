#include "mbed.h"
#include "config.h"
#include "sensors.h"
#include "dsp.h"
#include "gait.h"
// #include "ble_service.h"

DigitalOut led(LED1);   // status LED
const char *tremor_label(uint8_t level) {
    if (level < 10) {
        return "none";
    } else if (level < 30) {
        return "very mild";
    } else if (level < 60) {
        return "mild";
    } else if (level < 85) {
        return "moderate";
    } else {
        return "severe";
    }
}

int main() {
    printf("RTES pipeline starting...\n");
    led = 0;

    bool ok = true;
    ok &= sensors_init();
    dsp_init();
    gait_init();
    // ok &= ble_service_init();

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
            MovementAnalysis m = dsp_analyze_window(window);
            GaitStatus g = gait_update(window);

            printf("Window: tremor=%u (%s), dysk=%u, steps=%u spm, fog=%u, var=%u\n",
                   m.tremor_level,
                   tremor_label(m.tremor_level),
                   m.dyskinesia_level,
                   g.step_rate_spm,
                   g.fog_state,
                   g.variability);

            bool issue = (m.tremor_level > 0) ||
                         (m.dyskinesia_level > 0) ||
                         (g.fog_state != 0);
            led = issue ? 1 : 0;
        }

        ThisThread::sleep_for(20ms);
    }
}