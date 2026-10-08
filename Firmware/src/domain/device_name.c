#include "domain/device_name.h"

void device_name_format(const uint8_t mac[6], char *name)
{
    static const char hex[] = "0123456789ABCDEF";
    uint8_t i;

    name[0] = 'T';
    name[1] = 'H';
    name[2] = 'X';
    name[3] = '_';
    for (i = 0; i < 3; i++)
    {
        name[4 + i * 2] = hex[mac[2 - i] >> 4];
        name[5 + i * 2] = hex[mac[2 - i] & 0x0F];
    }
    name[DEVICE_NAME_LENGTH] = '\0';
}
