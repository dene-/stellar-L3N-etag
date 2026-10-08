#include "domain/refresh_policy.h"

// FNV-1a
static uint32_t frame_hash(const uint8_t *data, uint16_t size)
{
    uint32_t hash = 2166136261u;

    while (size--)
    {
        hash ^= *data++;
        hash *= 16777619u;
    }
    return hash;
}

refresh_kind_t refresh_policy_decide(refresh_policy_t *policy, const uint8_t *black, const uint8_t *red, uint16_t size,
                                     uint8_t redraw, uint8_t fast)
{
    uint32_t black_hash = frame_hash(black, size);
    uint32_t red_hash = frame_hash(red, size);
    uint8_t full;

    if (!redraw && policy->shown_valid && black_hash == policy->black_hash && red_hash == policy->red_hash)
        return REFRESH_SKIP;

    full = !policy->shown_valid || red_hash != policy->red_hash ||
           (!fast && (redraw || policy->partial_count >= REFRESH_POLICY_FULL_INTERVAL));
    if (full)
        policy->partial_count = 0;
    else if (policy->partial_count < REFRESH_POLICY_FULL_INTERVAL)
        policy->partial_count++;
    policy->shown_valid = 1;
    policy->black_hash = black_hash;
    policy->red_hash = red_hash;
    return full ? REFRESH_FULL : REFRESH_PARTIAL;
}

void refresh_policy_forget(refresh_policy_t *policy)
{
    policy->shown_valid = 0;
}
