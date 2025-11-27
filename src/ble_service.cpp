#include "ble_service.h"

// BLE Service Module - Currently not in use
// Detection results are transmitted via UART serial output instead
// To enable BLE functionality in the future:
// 1. Include mbed BLE headers (mbed_ble.h)
// 2. Define BLE service and characteristics for Tremor, Dyskinesia, Freezing
// 3. Implement ble_service_init() to initialize and start advertising
// 4. Implement ble_service_update() to write values to characteristics
// 5. Call ble_service_init() from main() and ble_service_update() from detect_symptoms()

bool ble_service_init() {
    return true;
}

void ble_service_update(const MovementAnalysis &m, const GaitStatus &g) {
    (void)m;
    (void)g;
}
