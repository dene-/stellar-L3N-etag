#include <stdint.h>
#include "tl_common.h"
#include "stack/ble/ble.h"
#include "application/display.h"
#include "application/screen.h"
#include "ble/ble.h"
#include "ble/epd_service.h"

// Raw frame upload. Commands (first byte):
//   00 <fill>                       start a frame: stop drawing scenes and fill both planes with <fill>
//   01 [full]                       show the frame (1 or omitted = full refresh, 0 = partial)
//   03 <plane> <off hi> <off lo> …  write bytes at offset into the black (plane FF) or red plane;
//                                   replies with the payload length, or 00 00 if out of range
int epd_service_write(void *p)
{
	rf_packet_att_write_t *req = (rf_packet_att_write_t *)p;
	uint8_t *payload = &req->value;
	unsigned int payload_len;
	uint8_t reply[2];

	if (req->l2capLen < 4) // ATT opcode + handle, then at least the command byte
		return 0;
	payload_len = req->l2capLen - 3;

	switch (payload[0])
	{
	case 0x00:
		if (payload_len < 2)
			return 0;
		screen_hold_frame();
		display_fill(payload[1], payload[1]);
		ble_set_connection_speed(40);
		break;
	case 0x01:
		ble_set_connection_speed(200);
		display_refresh(panel_plane_bytes(display_panel()), payload_len < 2 || payload[1]);
		break;
	case 0x03:
		if (payload_len < 5)
			return 0;
		if (display_write(payload[1] == 0xFF ? DISPLAY_PLANE_BLACK : DISPLAY_PLANE_RED,
						  (uint16_t)(payload[2] << 8 | payload[3]), payload + 4, payload_len - 4))
		{
			reply[0] = payload_len >> 8;
			reply[1] = payload_len & 0xFF;
		}
		else
		{
			reply[0] = 0x00;
			reply[1] = 0x00;
		}
		bls_att_pushNotifyData(EPD_BLE_CMD_OUT_DP_H, reply, 2);
		break;
	default:
		break;
	}
	return 0;
}
