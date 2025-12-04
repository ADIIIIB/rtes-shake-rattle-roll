// src/ble_service.cpp
#include "ble_service.h"

#include "mbed.h"
#include "ble/BLE.h"
#include "ble/gatt/GattService.h"
#include "ble/gatt/GattCharacteristic.h"
#include "ble/Gap.h"
#include "ble/gap/AdvertisingDataBuilder.h"
#include "events/EventQueue.h"

#include <cstdio>
#include <cstring>

using namespace ble;
using namespace events;

// -------- BLE globals --------

// BLE instance + event queue (runs in its own thread)
static BLE        &ble_interface = BLE::Instance();
static EventQueue  ble_event_queue;

// UUIDs for our custom service + characteristic
static const UUID TREMOR_SERVICE_UUID(
    "A0E1B2C3-D4E5-F6A7-B8C9-D0E1F2A3B4C5"
);
static const UUID STATUS_MSG_CHAR_UUID(
    "A1E2B3C4-D5E6-F7A8-B9C0-D1E2F3A4B5C6"
);

// Buffer for combined status string
static uint8_t status_buffer[80] = {0};
static uint8_t status_length     = 0;

// Notify-only characteristic carrying the status string
static ReadOnlyArrayGattCharacteristic<uint8_t, sizeof(status_buffer)>
    status_char(
        STATUS_MSG_CHAR_UUID,
        status_buffer,
        GattCharacteristic::BLE_GATT_CHAR_PROPERTIES_NOTIFY
    );

// GATT service
static GattCharacteristic *char_table[] = { &status_char };
static GattService tremor_service(
    TREMOR_SERVICE_UUID,
    char_table,
    sizeof(char_table) / sizeof(char_table[0])
);

// Forward declarations
static void update_status_message();
static void schedule_ble_events(BLE::OnEventsToProcessCallbackContext *ctx);
static void on_ble_init_complete(BLE::InitializationCompleteCallbackContext *params);

// Called when BLE has work to process – enqueue it
static void schedule_ble_events(BLE::OnEventsToProcessCallbackContext *ctx)
{
    ble_event_queue.call(mbed::callback(&ble_interface, &BLE::processEvents));
}

// ---- BLE init callback (adds service, sets advertising, etc.) ----

static void on_ble_init_complete(BLE::InitializationCompleteCallbackContext *params)
{
    if (params->error != BLE_ERROR_NONE) {
        printf("BLE init failed with error code %u\n", params->error);
        return;
    }

    printf("BLE initialized OK\n");

    // Add our service
    ble_interface.gattServer().addService(tremor_service);

    // Set up advertising payload
    Gap &gap = ble_interface.gap();

    static uint8_t adv_buffer[ble::LEGACY_ADVERTISING_MAX_SIZE];
    AdvertisingDataBuilder adv_data(adv_buffer);

    adv_data.clear();
    // Just advertise name + service UUID so phone apps can find it
    adv_data.setLocalServiceList(mbed::make_Span(&TREMOR_SERVICE_UUID, 1));
    adv_data.setName("GaitMate");

    gap.setAdvertisingParameters(
        LEGACY_ADVERTISING_HANDLE,
        AdvertisingParameters(
            advertising_type_t::CONNECTABLE_UNDIRECTED,
            adv_interval_t(160) // ~100 ms
        )
    );

    gap.setAdvertisingPayload(
        LEGACY_ADVERTISING_HANDLE,
        adv_data.getAdvertisingData()
    );

    gap.startAdvertising(LEGACY_ADVERTISING_HANDLE);

    printf("BLE advertising as GaitMate\n");
}

// ---- Actually send the current status_buffer via notification ----

static void update_status_message()
{
    // Always try to write; if no central is connected,
    // BLE stack will just return an error.
    status_length = strlen((char*)status_buffer);

    ble_error_t err = ble_interface.gattServer().write(
        status_char.getValueHandle(),
        status_buffer,
        status_length
    );

    if (err) {
        // This will often be "not connected" if no phone is attached
        printf("[BLE] write error: %u (msg=%s)\n", err, (char*)status_buffer);
    } else {
        printf("[BLE] Notification sent: %s\n", status_buffer);
    }
}

// -------- Public API used by your main.cpp --------

void ble_service_init()
{
    // Set up BLE event scheduling
    ble_interface.onEventsToProcess(schedule_ble_events);

    // Initialize BLE (async)
    ble_interface.init(on_ble_init_complete);

    // Run BLE event queue in its own thread
    static Thread ble_thread;
    ble_thread.start(callback(&ble_event_queue, &EventQueue::dispatch_forever));
}

// NOTE: adjust GaitStatus field names here to match your actual struct.
// From your logs, it looks like you have:
//   g.step_rate_spm   -> "steps=XX spm"
//   g.fog_state       -> "fog=0/1"
//   g.variability     -> "var=XX"
void ble_service_update(const MovementAnalysis &m,
                        const GaitStatus &g)
{
    // Build ONE human-readable string (Option A)
    snprintf(
        (char*)status_buffer,
        sizeof(status_buffer),
        "Tremor=%u,Dysk=%u,Steps=%u,Fog=%u,Var=%u",
        (unsigned)m.tremor_level,
        (unsigned)m.dyskinesia_level,
        (unsigned)g.step_rate_spm,   // <-- make sure this matches your GaitStatus
        (unsigned)g.fog_state,
        (unsigned)g.variability      // <-- was variability_score before; now fixed
    );

    // Schedule the actual notification on BLE thread
    ble_event_queue.call(update_status_message);
}