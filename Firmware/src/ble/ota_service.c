#include <stdint.h>
#include "tl_common.h"
#include "drivers.h"
#include "stack/ble/ble.h"
#include "ble/ble.h"
#include "ble/ota_service.h"
#include "domain/firmware_image.h"
#include "sections.h"

// Firmware update. The web flasher always addresses the image at OTA_STAGING_ADDRESS; the device
// writes it to the spare bank (the one it did not boot from), so the running firmware stays intact
// until the new one is complete and verified. The boot flag (byte 8) of the new image is held back
// until then: an interrupted upload leaves a bank the boot ROM ignores.
#define OTA_STAGING_ADDRESS 0x20000
#define OTA_LAST_PAGE (OTA_STAGING_ADDRESS + FIRMWARE_BANK_SIZE - 0x100) // never written

static RAM uint8_t ota_started = 0;
static RAM uint8_t out_buffer[20] = {0};
static RAM uint8_t ramd_to_flash_temp_buffer[0x100];
static RAM uint16_t ram_position = 0;
static RAM uint16_t crc_out = 0;
static RAM uint32_t spare_bank;
static RAM uint8_t image_flag = 0xFF; // byte 8 of the uploaded image, written last

_attribute_ram_code_ static void copy_to_bank_0_and_reboot(void);
_attribute_ram_code_ static void reboot(void);

static uint8_t staged(uint32_t address)
{
	return address >= OTA_STAGING_ADDRESS && address < OTA_LAST_PAGE;
}

static uint32_t to_spare_bank(uint32_t address)
{
	return spare_bank + (address - OTA_STAGING_ADDRESS);
}

// 16-bit byte sum of the whole image as sent (matches calculateCRC in the web flasher).
_attribute_ram_code_ static uint16_t ota_bank_checksum(void)
{
	uint16_t sum = 0;
	for (uint32_t i = 0; i < FIRMWARE_BANK_SIZE; i += 0x100)
	{
		flash_read_page(spare_bank + i, sizeof(ramd_to_flash_temp_buffer), ramd_to_flash_temp_buffer);
		if (i == 0)
			ramd_to_flash_temp_buffer[FIRMWARE_FLAG_OFFSET] = image_flag;
		for (int c = 0; c < 0x100; c++)
		{
			sum += ramd_to_flash_temp_buffer[c];
		}
	}
	return sum;
}

// Makes the staged bank the one the boot ROM starts. Returns only if that failed.
static void switch_bank_and_reboot(void)
{
	uint8_t flag = FIRMWARE_FLAG_BOOTABLE;
	uint8_t cleared = 0;

	flash_write_page(spare_bank + FIRMWARE_FLAG_OFFSET, 1, &flag);
	flash_read_page(spare_bank + FIRMWARE_FLAG_OFFSET, 1, &flag);
	if (flag != FIRMWARE_FLAG_BOOTABLE)
		return;
	// From here both banks are bootable until the old flag is cleared; either one starts fine.
	flash_write_page((spare_bank ^ FIRMWARE_BANK_SIZE) + FIRMWARE_FLAG_OFFSET, 1, &cleared);
	reboot();
}

static void install(void)
{
	uint8_t header[FIRMWARE_HEADER_SIZE];

	flash_read_page(spare_bank, sizeof(header), header);
	if (header[FIRMWARE_FLAG_OFFSET] != 0xFF) // a page program can only clear bits
		return;
	header[FIRMWARE_FLAG_OFFSET] = image_flag;
	switch (firmware_install_method(header, spare_bank))
	{
	case FIRMWARE_INSTALL_SWITCH_BANK:
		switch_bank_and_reboot();
		break;
	case FIRMWARE_INSTALL_COPY_TO_BANK_0:
		copy_to_bank_0_and_reboot();
		break;
	default:
		break;
	}
}

_attribute_ram_code_ void ota_service_reset(void)
{
	ota_started = 0;
}

_attribute_ram_code_ int ota_service_write(void *p)
{
	rf_packet_att_write_t *req = (rf_packet_att_write_t *)p;
	uint8_t *payload = &req->value;
	uint8_t data_len;
	uint32_t address = 0;

	if (req->l2capLen < 4) // ATT opcode + handle, then at least the command byte
		return 0;
	data_len = req->l2capLen - 3;

	if (!ota_started)
	{ // a short connection interval for the transfer
		uint8_t bank0_flag;

		ota_started = 1;
		ble_set_connection_speed(6);
		flash_read_page(FIRMWARE_FLAG_OFFSET, 1, &bank0_flag);
		spare_bank = firmware_spare_bank(bank0_flag);
	}
	if (data_len >= 5)
	{
		address = (payload[1] << 24) | (payload[2] << 16) | (payload[3] << 8) | payload[4];
	}

	switch (payload[0])
	{
	case 0: // just a reboot to test
		reboot();
		break;
	case 1: // erase the 4 KiB sector at a staging address
		crc_out = 0;
		if (staged(address))
		{
			flash_erase_sector(to_spare_bank(address));
			if (address - OTA_STAGING_ADDRESS < 0x1000)
				image_flag = 0xFF;
		}
		memset(ramd_to_flash_temp_buffer, 0x00, sizeof(ramd_to_flash_temp_buffer));
		ram_position = 0;
		bls_att_pushNotifyData(OTA_CMD_OUT_DP_H, payload, data_len);
		break;
	case 2: // write the page buffer to a staging address
		crc_out = 0;
		// inside one flash page (a page program wraps at the page end)
		if (staged(address) && (address & 0xFF) + ram_position <= 0x100)
		{
			uint32_t offset = address - OTA_STAGING_ADDRESS;

			if (offset <= FIRMWARE_FLAG_OFFSET && offset + ram_position > FIRMWARE_FLAG_OFFSET)
			{ // hold back the boot flag
				image_flag = ramd_to_flash_temp_buffer[FIRMWARE_FLAG_OFFSET - offset];
				ramd_to_flash_temp_buffer[FIRMWARE_FLAG_OFFSET - offset] = 0xFF;
			}
			flash_write_page(to_spare_bank(address), ram_position, ramd_to_flash_temp_buffer);
		}
		memset(ramd_to_flash_temp_buffer, 0x00, sizeof(ramd_to_flash_temp_buffer));
		ram_position = 0;
		break;
	case 3: // append to the page buffer
		crc_out = 0;
		if (ram_position + (data_len - 1) > 0x100)
			return 0;
		memcpy(&ramd_to_flash_temp_buffer[ram_position], &payload[1], (data_len - 1));
		ram_position += (data_len - 1);
		break;
	case 4: // read flash to verify; staging addresses read the spare bank
		crc_out = 0;
		flash_read_page(staged(address) ? to_spare_bank(address) : address, sizeof(out_buffer), out_buffer);
		bls_att_pushNotifyData(OTA_CMD_OUT_DP_H, out_buffer, sizeof(out_buffer));
		break;
	case 5: // read back the page buffer to verify
		crc_out = 0;
		if (address > sizeof(ramd_to_flash_temp_buffer) - sizeof(out_buffer))
			return 0;
		memcpy(out_buffer, &ramd_to_flash_temp_buffer[address], sizeof(out_buffer));
		bls_att_pushNotifyData(OTA_CMD_OUT_DP_H, out_buffer, sizeof(out_buffer));
		break;
	case 6: // checksum of the uploaded image; the web flasher still sends this before case 7 for old firmware
		crc_out = ota_bank_checksum();
		out_buffer[0] = 0x07;
		out_buffer[1] = crc_out >> 8;
		out_buffer[2] = crc_out;
		bls_att_pushNotifyData(OTA_CMD_OUT_DP_H, out_buffer, 3);
		break;
	case 7: // start the uploaded firmware: 07 C001CEED <crc hi> <crc lo>
		// On success the device reboots right away. Replies 07 00 <crc hi> <crc lo> on a checksum
		// mismatch, 07 00 for a malformed command or an image that is not bootable.
		out_buffer[0] = 0x07;
		out_buffer[1] = 0x00;
		if (address != 0xC001CEED || data_len < 7)
		{
			bls_att_pushNotifyData(OTA_CMD_OUT_DP_H, out_buffer, 2);
			break;
		}
		crc_out = ota_bank_checksum();
		if (crc_out == 0 || crc_out != ((payload[5] << 8) | payload[6]))
		{
			out_buffer[2] = crc_out >> 8;
			out_buffer[3] = crc_out;
			bls_att_pushNotifyData(OTA_CMD_OUT_DP_H, out_buffer, 4);
			break;
		}
		install();
		bls_att_pushNotifyData(OTA_CMD_OUT_DP_H, out_buffer, 2);
		break;
	}

	return 0;
}

_attribute_ram_code_ static void reboot(void)
{
	irq_disable();
	analog_write(SYS_DEEP_ANA_REG, analog_read(SYS_DEEP_ANA_REG) & (~SYS_NEED_REINIT_EXT32K));
	REG_ADDR8(0x6f) = 0x20;
	while (1)
	{
	}
}

// For images that cannot run from bank 0x20000, staged there while this firmware runs from bank 0.
// Rewrites the running bank from RAM with interrupts off; a power loss meanwhile leaves no firmware.
_attribute_ram_code_ static void copy_to_bank_0_and_reboot(void)
{
	irq_disable();
	uint32_t address = 0;
	while (address < FIRMWARE_BANK_SIZE)
	{
		flash_erase_sector(address);
		address += 0x1000;
	}
	address = 0;
	while (address < FIRMWARE_BANK_SIZE)
	{
		flash_read_page(spare_bank + address, 0x100, ramd_to_flash_temp_buffer);
		if (address == 0)
			ramd_to_flash_temp_buffer[FIRMWARE_FLAG_OFFSET] = image_flag;
		flash_write_page(address, 0x100, ramd_to_flash_temp_buffer);
		address += 0x100;
	}
	reboot();
}
