#include <stdint.h>
#include "tl_common.h"
#include "stack/ble/ble.h"
#include "application/device_settings.h"
#include "application/display.h"
#include "application/image_upload.h"
#include "application/screen.h"
#include "application/status_led.h"
#include "ble/ble.h"
#include "ble/rxtx_commands.h"
#include "application/local_time.h"
#include "domain/clock_schedule.h"

// RxTx characteristic: one command per write, opcode first. Replies go out as notifications.

typedef struct
{
	uint8_t opcode;
	uint8_t min_length; // shorter writes are ignored
	void (*handle)(const uint8_t *payload, uint16_t length);
} rxtx_command_t;

static void notify(const uint8_t *data, uint8_t length)
{
	bls_att_pushNotifyData(RxTx_CMD_OUT_DP_H, (uint8_t *)data, length);
}

static void notify_status(uint8_t command, uint8_t subcommand, uint8_t ok)
{
	uint8_t reply[3] = {command, subcommand, ok ? 0x01 : 0x00};
	notify(reply, sizeof(reply));
}

// B1 <pattern>: fill the panel with a byte pattern (test).
static void show_pattern(const uint8_t *payload, uint16_t length)
{
	display_show_pattern(payload[1]);
}

static uint32_t read_le32(const uint8_t *bytes)
{
	return (uint32_t)bytes[0] | ((uint32_t)bytes[1] << 8) | ((uint32_t)bytes[2] << 16) | ((uint32_t)bytes[3] << 24);
}

static uint16_t read_le16(const uint8_t *bytes)
{
	return bytes[0] | (bytes[1] << 8);
}

// DD <local time:4> <year:2> <month> <day> <weekday> (big endian; what firmware before the time
// zone support reads, ignored here), then little endian: <UTC seconds:4> <ms:2> <UTC offset
// minutes:2> <count> and count changes of <UTC seconds:4> <offset minutes:2> (at most
// TIME_ZONE_MAX_CHANGES, in order): set the clock and time zone.
#define SET_TIME_ZONE_START 19
static void set_time(const uint8_t *payload, uint16_t length)
{
	time_zone_t zone;
	uint8_t i;

	zone.offset_minutes = (int16_t)read_le16(&payload[16]);
	zone.change_count = payload[18];
	if (zone.change_count > TIME_ZONE_MAX_CHANGES || length < SET_TIME_ZONE_START + 6 * zone.change_count ||
		read_le16(&payload[14]) > 999)
		return;
	for (i = 0; i < zone.change_count; i++)
	{
		zone.changes[i].at = read_le32(&payload[SET_TIME_ZONE_START + 6 * i]);
		zone.changes[i].offset_minutes = (int16_t)read_le16(&payload[SET_TIME_ZONE_START + 6 * i + 4]);
	}
	local_time_sync(read_le32(&payload[10]), read_le16(&payload[14]), &zone);
}

// DE: restore and store the default settings, and apply them.
static void reset_settings(const uint8_t *payload, uint16_t length)
{
	device_settings_reset();
	screen_select_panel(device_settings_panel_model());
}

// DF: store the current settings now (they are also stored on disconnect).
static void save_settings(const uint8_t *payload, uint16_t length)
{
	device_settings_save();
}

// E0 <model>: select the panel model (PANEL_MODEL_*, 0 = auto-detect). Persisted.
static void select_panel(const uint8_t *payload, uint16_t length)
{
	if (payload[1] < PANEL_MODEL_COUNT)
		screen_select_panel(payload[1]);
}

// E1 <scene>: switch the scene (SCREEN_SCENE_*). E1 AA: query; replies E1 AA <scene>.
static void set_scene(const uint8_t *payload, uint16_t length)
{
	if (payload[1] == 0xAA)
	{
		uint8_t reply[3] = {0xE1, 0xAA, screen_scene()};

		notify(reply, sizeof(reply));
	}
	else if (payload[1] < SCREEN_SCENE_COUNT)
		screen_set_scene(payload[1]);
}

// E2 AA: report the panel temperature as int16 little endian in 0.1 degrees C.
// E2 AB: report E2 AB <model> <width:2> <height:2> <selected model> (little endian); <model> is the
// panel in use, <height> its visible rows, <selected model> the stored choice (PANEL_MODEL_AUTO when
// it was detected).
// E2 <other>: redraw the scene with a full refresh.
static void query_or_redraw(const uint8_t *payload, uint16_t length)
{
	if (payload[1] == 0xAA)
	{
		int16_t temperature_x10 = display_last_temperature();
		uint8_t reply[2] = {temperature_x10 & 0xFF, (temperature_x10 >> 8) & 0xFF};

		notify(reply, sizeof(reply));
	}
	else if (payload[1] == 0xAB)
	{
		const panel_t *panel = display_panel();
		uint8_t reply[8] = {0xE2, 0xAB, panel->model, panel->width & 0xFF, panel->width >> 8, panel->height & 0xFF,
							panel->height >> 8, device_settings_panel_model()};

		notify(reply, sizeof(reply));
	}
	else
	{
		screen_request_redraw();
	}
}

// E3 00|01: disable/enable the status LED (persisted). E3 AA: query; replies E3 AA <enabled>.
static void set_led_flashing(const uint8_t *payload, uint16_t length)
{
	if (payload[1] == 0xAA)
	{
		uint8_t reply[3] = {0xE3, 0xAA, device_settings_led_flashing_enabled()};

		notify(reply, sizeof(reply));
	}
	else if (payload[1] <= 0x01)
		device_settings_set_led_flashing_enabled(payload[1]);
}

// E4 00|01: stop/start the LED rainbow animation.
static void set_led_rainbow(const uint8_t *payload, uint16_t length)
{
	if (payload[1] <= 0x01)
		status_led_set_rainbow(payload[1]);
}

// E5 00 <model> <count> <interval:2 LE>: prepare an upload of count images; replies E5 00 <ok>.
// E5 01 <image> <plane> <offset:2 LE> <data…>: write a chunk; replies E5 01 00 only on failure.
// E5 02: commit and show the images; replies E5 02 <ok>.
// E5 03: delete the stored images and return to the dashboard; replies E5 03 01.
static void image_upload(const uint8_t *payload, uint16_t length)
{
	switch (payload[1])
	{
	case 0x00:
		if (length < 6)
		{
			notify_status(0xE5, 0x00, 0);
			return;
		}
		ble_set_connection_speed(6);
		notify_status(0xE5, 0x00, image_upload_begin(payload[2], payload[4] | (payload[5] << 8), payload[3]));
		break;
	case 0x01:
		if (length < 7 || !image_upload_write(payload[2], payload[3], payload[4] | (payload[5] << 8), &payload[6], length - 6))
			notify_status(0xE5, 0x01, 0);
		break;
	case 0x02:
		notify_status(0xE5, 0x02, image_upload_finish());
		ble_set_connection_speed(200);
		break;
	case 0x03:
		image_upload_clear();
		ble_set_connection_speed(200);
		notify_status(0xE5, 0x03, 1);
		break;
	default:
		break;
	}
}

// E6 00|01: disable/enable fast refresh (persisted); E6 AA: query.
// Replies E6 <enabled> <supported> (every panel supports partial refresh).
static void fast_refresh(const uint8_t *payload, uint16_t length)
{
	uint8_t reply[3];

	if (payload[1] <= 0x01)
		device_settings_set_fast_refresh_enabled(payload[1]);
	else if (payload[1] != 0xAA)
		return;

	reply[0] = 0xE6;
	reply[1] = device_settings_fast_refresh_enabled();
	reply[2] = 1;
	notify(reply, sizeof(reply));
}

// E7 <minutes> <sync>: show a new clock frame every 1 to 60 minutes, counted from midnight, and
// with sync 01 start each refresh early so it ends as the minute changes (persisted); E7 AA: query.
// Replies E7 <minutes> <sync>.
static void clock_schedule(const uint8_t *payload, uint16_t length)
{
	uint8_t reply[3];

	if (payload[1] != 0xAA)
	{
		if (length < 3 || !clock_schedule_valid(payload[1]) || payload[2] > 0x01)
			return;
		device_settings_set_clock_interval(payload[1]);
		device_settings_set_clock_sync(payload[2]);
		screen_clock_schedule_changed();
	}

	reply[0] = 0xE7;
	reply[1] = device_settings_clock_interval();
	reply[2] = device_settings_clock_sync();
	notify(reply, sizeof(reply));
}

// Release version, e.g. "0.10.0"; CI passes it to make (FIRMWARE_VERSION), local builds the git
// description.
#ifndef FIRMWARE_VERSION
#define FIRMWARE_VERSION "dev"
#endif

// E8: replies E8 <version, ASCII, at most 19 characters>.
static void report_version(const uint8_t *payload, uint16_t length)
{
	static const char version[] = FIRMWARE_VERSION;
	uint8_t reply[20] = {0xE8};
	uint8_t count = 0;

	while (version[count] && count < sizeof(reply) - 1)
	{
		reply[1 + count] = (uint8_t)version[count];
		count++;
	}
	notify(reply, 1 + count);
}

// E9 AA: replies E9 AA <stored images> <slideshow interval:2 LE>, in seconds (0 = a minute).
// E9 01 <interval:2 LE>: change the slideshow interval of the stored images; replies like E9 AA.
static void stored_images(const uint8_t *payload, uint16_t length)
{
	uint16_t interval;
	uint8_t reply[5];

	if (payload[1] == 0x01)
	{
		if (length < 4)
			return;
		image_upload_set_interval(payload[2] | (payload[3] << 8));
	}
	else if (payload[1] != 0xAA)
		return;

	interval = image_upload_interval();
	reply[0] = 0xE9;
	reply[1] = 0xAA;
	reply[2] = image_upload_stored_count();
	reply[3] = interval & 0xFF;
	reply[4] = interval >> 8;
	notify(reply, sizeof(reply));
}

static const rxtx_command_t commands[] = {
	{0xB1, 2, show_pattern},
	{0xDD, SET_TIME_ZONE_START, set_time},
	{0xDE, 1, reset_settings},
	{0xDF, 1, save_settings},
	{0xE0, 2, select_panel},
	{0xE1, 2, set_scene},
	{0xE2, 2, query_or_redraw},
	{0xE3, 2, set_led_flashing},
	{0xE4, 2, set_led_rainbow},
	{0xE5, 2, image_upload},
	{0xE6, 2, fast_refresh},
	{0xE7, 2, clock_schedule},
	{0xE8, 1, report_version},
	{0xE9, 2, stored_images},
};

_attribute_ram_code_ int rxtx_commands_write(void *p)
{
	rf_packet_att_write_t *req = (rf_packet_att_write_t *)p;
	const uint8_t *payload = &req->value;
	uint16_t length;
	unsigned int i;

	if (req->l2capLen < 4) // ATT opcode + handle, then at least the command byte
		return 0;
	length = req->l2capLen - 3;
	for (i = 0; i < sizeof(commands) / sizeof(commands[0]); i++)
	{
		if (commands[i].opcode == payload[0])
		{
			if (length >= commands[i].min_length)
				commands[i].handle(payload, length);
			break;
		}
	}
	return 0;
}
