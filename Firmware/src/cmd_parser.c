#include <stdint.h>
#include "main.h"
#include "epd.h"
#include "tl_common.h"
#include "stack/ble/ble.h"
#include "vendor/common/blt_common.h"

#include "etime.h"
#include "flash.h"
#include "led.h"

extern settings_struct settings;
extern uint8_t epd_temperature; // last measured EPD temperature (°C)

#define testPin GPIO_PD3
void cmd_parser(void *p)
{
	rf_packet_att_data_t *req = (rf_packet_att_data_t *)p;
	uint8_t inData = req->dat[0];
	if (inData == 0xFF)
	{
		gpio_set_func(testPin, AS_GPIO);
		gpio_set_output_en(testPin, 1);
		gpio_set_input_en(testPin, 0);
		sleep_ms(500);
	}
	else if (inData == 0xCC)
	{
		gpio_set_func(GPIO_PD2, AS_GPIO);
		gpio_set_output_en(GPIO_PD2, 1);
		gpio_set_input_en(GPIO_PD2, 0);
		sleep_ms(500);
	}
	else if (inData == 0x0F)
	{
		settings.advertising_temp_C_or_F = true; // Advertising Temp in F
	}
	else if (inData == 0x0C)
	{
		settings.advertising_temp_C_or_F = false; // Advertising Temp in C
	}
	else if (inData == 0xB1)
	{
		epd_display_char(req->dat[1]);
	}
	else if (inData == 0xFE)
	{
		settings.advertising_interval = req->dat[1]; // Set advertising interval with second byte, value*10second / 0=main_delay
	}
	else if (inData == 0xFA)
	{
		settings.temp_offset = req->dat[1]; // Set temp offset, -12,5 - +12,5 °C
	}
	else if (inData == 0xFC)
	{
		settings.temp_alarm_point = req->dat[1]; // Set temp alarm point value divided by 10 for temp in °C
		if (settings.temp_alarm_point == 0)
			settings.temp_alarm_point = 1;
	}
	else if (inData == 0xDD)
	{ // Set time
		uint32_t new_time = (req->dat[1] << 24) + (req->dat[2] << 16) + (req->dat[3] << 8) + (req->dat[4] & 0xff);
		set_time(new_time, (req->dat[5] << 8) + req->dat[6], req->dat[7], req->dat[8], req->dat[9]);
	}
	else if (inData == 0xDE)
	{ // Save settings in flash to default
		reset_settings_to_default();
		save_settings_to_flash();
	}
	else if (inData == 0xDF)
	{ // Save current settings in flash
		save_settings_to_flash();
	}
	else if (inData == 0xE0)
	{ // force set an EPD model, if it wasnt detect automatically correct
		set_EPD_model(req->dat[1]);
	}
	else if (inData == 0xE1)
	{ // force set an EPD scene
		set_EPD_scene(req->dat[1]);
	}
	else if (inData == 0xE2)
	{
		// If second byte is 0xAA treat as a temperature query over RxTx
		if (req->dat[1] == 0xAA)
		{
			// Package epd_temperature as signed int16 little-endian *10 for one decimal resolution
			int16_t t10 = (int16_t)epd_temperature * 10;
			u8 buf[2] = {(u8)(t10 & 0xFF), (u8)((t10 >> 8) & 0xFF)};
			bls_att_pushNotifyData(RxTx_CMD_OUT_DP_H, buf, 2);
		}
		else if (req->dat[1] == 0xAB)
		{
			uint16_t width = 0;
			uint16_t height = 0;
			uint8_t model = get_EPD_model();

			if (!model)
			{
				EPD_detect_model();
				model = get_EPD_model();
			}

			epd_get_current_resolution(&width, &height);

			u8 buf[7] = {
				0xE2,
				0xAB,
				model,
				(u8)(width & 0xFF),
				(u8)((width >> 8) & 0xFF),
				(u8)(height & 0xFF),
				(u8)((height >> 8) & 0xFF),
			};
			bls_att_pushNotifyData(RxTx_CMD_OUT_DP_H, buf, 7);
		}
		else
		{
			set_EPD_wait_flush();
		}
	}
	else if (inData == 0xE3)
	{ // Toggle LED flashing enable: E3 00 -> disable, E3 01 -> enable
		if (req->dat[1] == 0x00)
		{
			settings.led_flashing_enabled = 0;
		}
		else if (req->dat[1] == 0x01)
		{
			settings.led_flashing_enabled = 1;
		}
		// Optionally persist immediately
		save_settings_to_flash();
	}
	else if (inData == 0xE4)
	{ // LED rainbow mode: E4 00 -> disable, E4 01 -> enable
		if (req->dat[1] == 0x00)
		{
			led_set_rainbow_enabled(0);
		}
		else if (req->dat[1] == 0x01)
		{
			led_set_rainbow_enabled(1);
		}
	}
}
