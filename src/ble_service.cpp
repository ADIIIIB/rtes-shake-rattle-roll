#include "ble_service.h"

bool ble_service_init() {
    // No BLE stack yet; just pretend success
    return true;
}

void ble_service_update(const MovementAnalysis &m, const GaitStatus &g) {
    (void)m;
    (void)g;
    // In the future: push values to BLE characteristics
}