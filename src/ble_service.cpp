#include "ble_service.h"

// TODO: include mbed BLE headers and define service/characteristics here.

bool ble_service_init() {
    // TODO: init BLE, start advertising, etc.
    return true;
}

void ble_service_update(const MovementAnalysis &m, const GaitStatus &g) {
    (void)m;
    (void)g;
    // TODO: write tremor_level, dyskinesia_level, fog_state to BLE characteristics
}
