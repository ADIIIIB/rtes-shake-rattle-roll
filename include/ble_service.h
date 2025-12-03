#pragma once
#include "dsp.h"
#include "gait.h"

bool ble_service_init();
void ble_service_update(const MovementAnalysis &m, const GaitStatus &g);