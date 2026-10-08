#include "domain/time_zone.h"

int32_t time_zone_offset_seconds(const time_zone_t *zone, uint32_t utc)
{
    int16_t offset = zone->offset_minutes;
    uint8_t i;

    for (i = 0; i < zone->change_count && i < TIME_ZONE_MAX_CHANGES && utc >= zone->changes[i].at; i++)
        offset = zone->changes[i].offset_minutes;
    return (int32_t)offset * 60;
}
