#include "domain/firmware_image.h"

uint32_t firmware_spare_bank(uint8_t bank0_flag)
{
    return bank0_flag == FIRMWARE_FLAG_BOOTABLE ? FIRMWARE_BANK_SIZE : 0;
}

firmware_install_t firmware_install_method(const uint8_t *header, uint32_t staged_bank)
{
    const uint8_t *marker = &header[FIRMWARE_DUAL_BANK_OFFSET];
    uint32_t dual_bank = (uint32_t)marker[0] | ((uint32_t)marker[1] << 8) | ((uint32_t)marker[2] << 16) |
                         ((uint32_t)marker[3] << 24);

    // "KNLT": the signature the boot ROM looks for
    if (header[8] != FIRMWARE_FLAG_BOOTABLE || header[9] != 0x4E || header[10] != 0x4C || header[11] != 0x54)
        return FIRMWARE_INSTALL_REJECT;
    if (staged_bank == 0 || dual_bank == FIRMWARE_DUAL_BANK_MAGIC)
        return FIRMWARE_INSTALL_SWITCH_BANK;
    return FIRMWARE_INSTALL_COPY_TO_BANK_0;
}
