#include <stdint.h>
#include "tl_common.h"
#include "drivers.h"
#include "stack/ble/ble.h"
#include "vendor/common/blt_common.h"
#include "application/image_upload.h"
#include "application/status_led.h"
#include "application/ports/telemetry_sink.h"
#include "ble/ble.h"
#include "ble/gatt.h"
#include "ble/ota_service.h"
#include "domain/device_name.h"
#include "sections.h"

static RAM uint8_t ble_connected = 0;

RAM uint8_t blt_rxfifo_b[64 * 8] = {0};
RAM my_fifo_t blt_rxfifo = {
		64,
		8,
		0,
		0,
		blt_rxfifo_b,
};

RAM uint8_t blt_txfifo_b[40 * 16] = {0};
RAM my_fifo_t blt_txfifo = {
		40,
		16,
		0,
		0,
		blt_txfifo_b,
};

// Scan response: complete local name (AD type 0x09).
static RAM uint8_t scan_response[2 + DEVICE_NAME_LENGTH] = {DEVICE_NAME_LENGTH + 1, 0x09};
static RAM char device_name[DEVICE_NAME_LENGTH + 1];
static RAM uint8_t mac_public[6]; // the stack keeps using it after ble_init()
static RAM uint8_t advertising_data[] = {
		/*Description*/ 16, 0x16, 0x1a, 0x18,
		/*MAC*/ 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
		/*Temp*/ 0xaa, 0xaa,
		/*Humi*/ 0xbb,
		/*BatL*/ 0xcc,
		/*BatM*/ 0xdd, 0xdd,
		/*Counter*/ 0x00};

_attribute_ram_code_ static void ble_disconnect_callback(uint8_t e, uint8_t *p, int n)
{
	ble_connected = 0;
	ota_service_reset();
	// Settings are stored by the main loop, not here. A half-sent image upload is dropped: its
	// store stays empty (the header was erased when it started).
	image_upload_abort();
	status_led_set_rainbow(0);
	printf("BLE disconnected\r\n");
}

_attribute_ram_code_ static void user_set_rf_power(uint8_t e, uint8_t *p, int n)
{
	rf_set_power_level_index(RF_POWER_P3p01dBm);
}

_attribute_ram_code_ static void ble_connect_callback(uint8_t e, uint8_t *p, int n)
{
	ble_connected = 1;
	ota_service_reset();
	ble_set_connection_speed(200);
	printf("BLE connected\r\n");
}

_attribute_ram_code_ void ble_set_connection_speed(uint16_t speed)
{
	bls_l2cap_requestConnParamUpdate(speed, speed + 2, 0, 2000);
}

void ble_init(void)
{
	uint8_t mac_random_static[6];
	uint8_t i;

	blc_initMacAddress(CFG_ADR_MAC, mac_public, mac_random_static);

	device_name_format(mac_public, device_name);
	for (i = 0; i < DEVICE_NAME_LENGTH; i++)
		scan_response[2 + i] = device_name[i];
	for (i = 0; i < 6; i++)
		advertising_data[4 + i] = mac_public[5 - i];

	////// Controller Initialization  //////////
	blc_ll_initBasicMCU();										 // must
	blc_ll_initStandby_module(mac_public);		 // must
	blc_ll_initAdvertising_module(mac_public); // adv module: 		 must for BLE slave,
	blc_ll_initConnection_module();						 // connection module  must for BLE slave/master
	blc_ll_initSlaveRole_module();						 // slave module: 	 must for BLE slave,

	////// Host Initialization  //////////
	blc_gap_peripheral_init();
	gatt_init(device_name);
	blc_l2cap_register_handler(blc_l2cap_packet_receive);
	blc_smp_setSecurityLevel(No_Security);

	///////////////////// USER application initialization ///////////////////
	bls_ll_setScanRspData(scan_response, sizeof(scan_response));
	bls_ll_setAdvParam(ADVERTISING_INTERVAL, ADVERTISING_INTERVAL + 50, ADV_TYPE_CONNECTABLE_UNDIRECTED, OWN_ADDRESS_PUBLIC, 0, NULL, BLT_ENABLE_ADV_ALL, ADV_FP_NONE);
	bls_ll_setAdvEnable(1);
	user_set_rf_power(0, 0, 0);
	bls_app_registerEventCallback(BLT_EV_FLAG_SUSPEND_EXIT, &user_set_rf_power);
	bls_app_registerEventCallback(BLT_EV_FLAG_CONNECT, &ble_connect_callback);
	bls_app_registerEventCallback(BLT_EV_FLAG_TERMINATE, &ble_disconnect_callback);

	///////////////////// Power Management initialization///////////////////
	blc_ll_initPowerManagement_module();
	bls_pm_setSuspendMask(SUSPEND_ADV | DEEPSLEEP_RETENTION_ADV | SUSPEND_CONN | DEEPSLEEP_RETENTION_CONN);
	blc_pm_setDeepsleepRetentionThreshold(95, 95);
	blc_pm_setDeepsleepRetentionEarlyWakeupTiming(240);
	blc_pm_setDeepsleepRetentionType(DEEPSLEEP_MODE_RET_SRAM_LOW32K);
	blc_att_setRxMtuSize(250);
}

_attribute_ram_code_ uint8_t ble_is_connected(void)
{
	return ble_connected;
}

const char *ble_device_name(void)
{
	return device_name;
}

_attribute_ram_code_ void telemetry_sink_publish(int16_t temperature_x10, uint8_t battery_percent, uint16_t battery_mv)
{
	advertising_data[10] = temperature_x10 >> 8;
	advertising_data[11] = temperature_x10 & 0xff;
	advertising_data[13] = battery_percent;
	advertising_data[14] = battery_mv >> 8;
	advertising_data[15] = battery_mv & 0xff;
	advertising_data[16]++;
	bls_ll_setAdvData(advertising_data, sizeof(advertising_data));

	gatt_notify_battery(battery_percent);
	gatt_notify_temperature(temperature_x10);
}
