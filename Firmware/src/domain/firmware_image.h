#pragma once
#include <stdint.h>

// Firmware images and the two flash banks the TLSR825x boot ROM starts them from. The ROM boots
// the bank at 0x00000 if byte 8 there is FIRMWARE_FLAG_BOOTABLE, else the one at 0x20000, and maps
// that bank to address 0, so the same image runs from either.
#define FIRMWARE_BANK_SIZE 0x20000
#define FIRMWARE_HEADER_SIZE 0x20
#define FIRMWARE_FLAG_OFFSET 8
#define FIRMWARE_FLAG_BOOTABLE 0x4B
// Word at 0x1C of images that support running from bank 0x20000 (see static_src/cstartup_825x.S).
// Older images always stage updates at 0x20000 and so must run from bank 0.
#define FIRMWARE_DUAL_BANK_OFFSET 0x1C
#define FIRMWARE_DUAL_BANK_MAGIC 0x4B4E4232UL // "2BNK"

typedef enum
{
    FIRMWARE_INSTALL_REJECT,         // not a bootable image
    FIRMWARE_INSTALL_SWITCH_BANK,    // mark the staged bank bootable and the running one not
    FIRMWARE_INSTALL_COPY_TO_BANK_0, // overwrite the running bank 0 (no fallback if power fails)
} firmware_install_t;

// The bank the running firmware was not started from, where an update is staged: bank0_flag is
// byte FIRMWARE_FLAG_OFFSET of flash address 0.
uint32_t firmware_spare_bank(uint8_t bank0_flag);
// How to start the image staged in staged_bank, given its first FIRMWARE_HEADER_SIZE bytes as sent.
firmware_install_t firmware_install_method(const uint8_t *header, uint32_t staged_bank);
