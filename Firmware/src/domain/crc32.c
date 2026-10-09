#include "domain/crc32.h"

// Two table steps of 4 bits per byte: a 64-byte table instead of 1 KiB, and still much faster than
// going bit by bit over the whole image store.
static const uint32_t nibble_table[16] = {
    0x00000000, 0x1DB71064, 0x3B6E20C8, 0x26D930AC, 0x76DC4190, 0x6B6B51F4, 0x4DB26158, 0x5005713C,
    0xEDB88320, 0xF00F9344, 0xD6D6A3E8, 0xCB61B38C, 0x9B64C2B0, 0x86D3D2D4, 0xA00AE278, 0xBDBDF21C,
};

uint32_t crc32_update(uint32_t crc, const uint8_t *data, uint32_t length)
{
    crc = ~crc;
    while (length--)
    {
        crc ^= *data++;
        crc = (crc >> 4) ^ nibble_table[crc & 0x0F];
        crc = (crc >> 4) ^ nibble_table[crc & 0x0F];
    }
    return ~crc;
}
