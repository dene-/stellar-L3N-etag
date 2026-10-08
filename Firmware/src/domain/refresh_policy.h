#pragma once
#include <stdint.h>

// Decides how to show a periodically redrawn frame (the clock scenes): skip it when it matches the
// frame on the panel, and otherwise
//   - full refresh when the panel content is unknown or red content changed (a partial refresh
//     cannot draw red), always;
//   - full refresh for a requested redraw and after REFRESH_POLICY_FULL_INTERVAL partial ones
//     (against ghosting), unless fast mode is on;
//   - partial refresh otherwise.
#define REFRESH_POLICY_FULL_INTERVAL 10

typedef enum
{
    REFRESH_SKIP = 0,
    REFRESH_PARTIAL,
    REFRESH_FULL,
} refresh_kind_t;

typedef struct
{
    uint8_t shown_valid; // 0 when the panel shows something this policy did not decide on
    uint32_t black_hash;
    uint32_t red_hash;
    uint8_t partial_count;
} refresh_policy_t;

// Decides for the frame in black/red (size bytes each) and, unless skipped, records it as shown.
// redraw refreshes even an unchanged frame.
refresh_kind_t refresh_policy_decide(refresh_policy_t *policy, const uint8_t *black, const uint8_t *red, uint16_t size,
                                     uint8_t redraw, uint8_t fast);
// What refresh_policy_decide would decide for the frame, without recording anything.
refresh_kind_t refresh_policy_peek(const refresh_policy_t *policy, const uint8_t *black, const uint8_t *red, uint16_t size,
                                   uint8_t redraw, uint8_t fast);
// Something else was drawn on the panel; the next frame gets a full refresh.
void refresh_policy_forget(refresh_policy_t *policy);
