// domain/crc32: the standard CRC-32 and feeding the data in pieces.
#include <string.h>
#include "check.h"
#include "domain/crc32.h"

int main(void)
{
    static const uint8_t check[] = "123456789";
    uint8_t flash[300];
    uint32_t crc;
    unsigned int i;

    CHECK_EQ(crc32_update(0, check, 9), 0xCBF43926u);
    CHECK_EQ(crc32_update(0, check, 0), 0);

    // Pieces of any size give the CRC of the whole, as when the data is read from flash page by page.
    crc = crc32_update(0, check, 4);
    crc = crc32_update(crc, check + 4, 1);
    crc = crc32_update(crc, check + 5, 4);
    CHECK_EQ(crc, 0xCBF43926u);

    // Erased flash is part of the data: 0xFF bytes are not skipped.
    memset(flash, 0xFF, sizeof(flash));
    CHECK_EQ(crc32_update(0, flash, 32), 0xFF6CAB0Bu);
    crc = 0;
    for (i = 0; i < sizeof(flash); i += 100)
        crc = crc32_update(crc, flash + i, 100);
    CHECK_EQ(crc, crc32_update(0, flash, sizeof(flash)));
    flash[299] = 0xFE;
    CHECK(crc32_update(0, flash, sizeof(flash)) != crc);

    return check_report();
}
