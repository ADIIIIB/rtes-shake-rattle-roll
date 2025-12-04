// include/ble_service.h
#pragma once

#include "dsp.h"
#include "gait.h"

// Initialize BLE system, start advertising, etc.
// Call once from main() after sensors/dsp/gait init.
void ble_service_init();

// Update the BLE status characteristic with the latest values.
// You’ll call this from your main loop after computing m and g.
void ble_service_update(const MovementAnalysis &m,
                        const GaitStatus &g);