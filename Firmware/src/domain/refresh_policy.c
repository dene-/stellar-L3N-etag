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

static refresh_kind_t decide(const refresh_policy_t *policy, uint32_t black_hash, uint32_t red_hash, uint8_t redraw,
                             uint8_t fast)
{
    if (!redraw && policy->shown_valid && black_hash == policy->black_hash && red_hash == policy->red_hash)
        return REFRESH_SKIP;
    if (redraw || !policy->shown_valid || red_hash != policy->red_hash ||
        (!fast && policy->partial_count >= REFRESH_POLICY_FULL_INTERVAL))
        return REFRESH_FULL;
    return REFRESH_PARTIAL;
}

refresh_kind_t refresh_policy_peek(const refresh_policy_t *policy, const uint8_t *black, const uint8_t *red, uint16_t size,
                                   uint8_t redraw, uint8_t fast)
{
    return decide(policy, frame_hash(black, size), frame_hash(red, size), redraw, fast);
}

refresh_kind_t refresh_policy_decide(refresh_policy_t *policy, const uint8_t *black, const uint8_t *red, uint16_t size,
                                     uint8_t redraw, uint8_t fast)
{
    uint32_t black_hash = frame_hash(black, size);
    uint32_t red_hash = frame_hash(red, size);
    refresh_kind_t kind = decide(policy, black_hash, red_hash, redraw, fast);

    if (kind == REFRESH_SKIP)
        return kind;
    if (kind == REFRESH_FULL)
        policy->partial_count = 0;
    else if (policy->partial_count < REFRESH_POLICY_FULL_INTERVAL)
        policy->partial_count++;
    policy->shown_valid = 1;
    policy->black_hash = black_hash;
    policy->red_hash = red_hash;
    return kind;
}

void refresh_policy_forget(refresh_policy_t *policy)
{
    policy->shown_valid = 0;
}
