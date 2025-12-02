#include "ble_service.h"
#include "mbed.h"
#include "ble/BLE.h"
#include "ble/Gap.h"
#include "ble/GattServer.h"
#include "ble/GattCharacteristic.h"

// BLE Service Module - Parkinson's Symptom Detection
// Detection results are transmitted via both UART and BLE

// Custom UUIDs for Parkinson's Detection Service
// Base UUID: 0000xxxx-0000-1000-8000-00805f9b34fb (standard BLE format)
const uint16_t PARKINSONS_SERVICE_UUID = 0xA000;
const uint16_t TREMOR_CHAR_UUID = 0xA001;
const uint16_t DYSKINESIA_CHAR_UUID = 0xA002;
const uint16_t FREEZING_CHAR_UUID = 0xA003;

// BLE objects
static BLE *ble_instance = nullptr;
static GattCharacteristic *tremor_char = nullptr;
static GattCharacteristic *dyskinesia_char = nullptr;
static GattCharacteristic *freezing_char = nullptr;

// Characteristic values (1 byte each, 0-100 range)
static uint8_t tremor_value = 0;
static uint8_t dyskinesia_value = 0;
static uint8_t freezing_value = 0;

// BLE event callbacks
static void on_ble_init_complete(BLE::InitializationCompleteCallbackContext *context) {
    if (context->error != BLE_ERROR_NONE) {
        printf("BLE initialization failed with error %d\r\n", context->error);
        return;
    }

    printf("BLE initialized successfully\r\n");

    // Setup advertising
    ble_instance = &BLE::Instance();
    ble::Gap &gap = ble_instance->gap();

    // Set device name
    const char DEVICE_NAME[] = "GaitWave";
    gap.setDeviceName((const uint8_t *)DEVICE_NAME, sizeof(DEVICE_NAME));

    // Configure advertising data
    ble::AdvertisingParameters adv_params(
        ble::advertising_type_t::CONNECTABLE_UNDIRECTED,
        ble::adv_interval_t(ble::millisecond_t(1000))
    );

    // Setup advertising payload
    ble::AdvertisingDataBuilder adv_data_builder;
    adv_data_builder.setFlags();
    adv_data_builder.setName(DEVICE_NAME);

    // Start advertising
    ble_error_t error = gap.setAdvertisingParameters(
        ble::LEGACY_ADVERTISING_HANDLE,
        adv_params
    );

    if (error != BLE_ERROR_NONE) {
        printf("Failed to set advertising parameters: %d\r\n", error);
        return;
    }

    error = gap.setAdvertisingPayload(
        ble::LEGACY_ADVERTISING_HANDLE,
        adv_data_builder.getAdvertisingData()
    );

    if (error != BLE_ERROR_NONE) {
        printf("Failed to set advertising payload: %d\r\n", error);
        return;
    }

    error = gap.startAdvertising(ble::LEGACY_ADVERTISING_HANDLE);

    if (error != BLE_ERROR_NONE) {
        printf("Failed to start advertising: %d\r\n", error);
        return;
    }

    printf("BLE advertising started: %s\r\n", DEVICE_NAME);
}

bool ble_service_init() {
    printf("Initializing BLE service...\r\n");

    // Get BLE instance
    ble_instance = &BLE::Instance();

    // Define characteristics with read and notify properties
    tremor_char = new GattCharacteristic(
        TREMOR_CHAR_UUID,
        &tremor_value,
        sizeof(tremor_value),
        sizeof(tremor_value),
        GattCharacteristic::BLE_GATT_CHAR_PROPERTIES_READ |
        GattCharacteristic::BLE_GATT_CHAR_PROPERTIES_NOTIFY
    );

    dyskinesia_char = new GattCharacteristic(
        DYSKINESIA_CHAR_UUID,
        &dyskinesia_value,
        sizeof(dyskinesia_value),
        sizeof(dyskinesia_value),
        GattCharacteristic::BLE_GATT_CHAR_PROPERTIES_READ |
        GattCharacteristic::BLE_GATT_CHAR_PROPERTIES_NOTIFY
    );

    freezing_char = new GattCharacteristic(
        FREEZING_CHAR_UUID,
        &freezing_value,
        sizeof(freezing_value),
        sizeof(freezing_value),
        GattCharacteristic::BLE_GATT_CHAR_PROPERTIES_READ |
        GattCharacteristic::BLE_GATT_CHAR_PROPERTIES_NOTIFY
    );

    // Create service with characteristics
    GattCharacteristic *chars[] = {tremor_char, dyskinesia_char, freezing_char};
    GattService parkinsons_service(PARKINSONS_SERVICE_UUID, chars, 3);

    // Add service to GATT server
    ble_instance->gattServer().addService(parkinsons_service);

    // Initialize BLE
    ble_error_t error = ble_instance->init(on_ble_init_complete);

    if (error != BLE_ERROR_NONE) {
        printf("BLE init failed with error %d\r\n", error);
        return false;
    }

    printf("BLE service initialization started\r\n");
    return true;
}

void ble_service_update(const MovementAnalysis &m, const GaitStatus &g) {
    if (ble_instance == nullptr || tremor_char == nullptr) {
        return; // BLE not initialized
    }

    // Update tremor characteristic (0-100)
    tremor_value = m.tremor_level;
    ble_instance->gattServer().write(
        tremor_char->getValueHandle(),
        &tremor_value,
        sizeof(tremor_value)
    );

    // Update dyskinesia characteristic (0-100)
    dyskinesia_value = m.dyskinesia_level;
    ble_instance->gattServer().write(
        dyskinesia_char->getValueHandle(),
        &dyskinesia_value,
        sizeof(dyskinesia_value)
    );

    // Update freezing characteristic (0-100)
    // fog_state: 0 = none, 1 = freeze start, 2 = sustained freeze
    // Convert to 0-100 scale: 0 = 0, 1 = 50, 2 = 100
    freezing_value = (g.fog_state == 0) ? 0 : ((g.fog_state == 1) ? 50 : 100);
    ble_instance->gattServer().write(
        freezing_char->getValueHandle(),
        &freezing_value,
        sizeof(freezing_value)
    );
}
