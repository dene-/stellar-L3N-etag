#pragma once
#include <stdint.h>

// Standard CRC-32 (reflected, polynomial 0xEDB88320, init and final xor 0xFFFFFFFF; "123456789"
// gives 0xCBF43926). `crc` is the CRC of the data so far, 0 at the start, so a long input can be
// fed in pieces: crc = crc32_update(crc, piece, length).
uint32_t crc32_update(uint32_t crc, const uint8_t *data, uint32_t length);
