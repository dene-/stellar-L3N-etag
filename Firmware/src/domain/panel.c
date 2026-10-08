#include "domain/panel.h"

// The 2.13" panels have 122 visible rows; their controllers keep 128 (16 bytes) per column.
static const panel_t panels[PANEL_MODEL_COUNT] = {
    [PANEL_MODEL_BW213] = {PANEL_MODEL_BW213, "BW213", 250, 122, 0, 1},
    [PANEL_MODEL_BWR213] = {PANEL_MODEL_BWR213, "BWR213", 250, 122, 1, 1},
    [PANEL_MODEL_BWR154] = {PANEL_MODEL_BWR154, "BWR154", 200, 200, 1, 1},
    [PANEL_MODEL_BW213_ICE] = {PANEL_MODEL_BW213_ICE, "213ICE", 212, 104, 0, 1},
    [PANEL_MODEL_BWR296] = {PANEL_MODEL_BWR296, "BWR296", 296, 128, 1, 1},
    [PANEL_MODEL_BW296] = {PANEL_MODEL_BW296, "BW296", 296, 128, 0, 1},
    [PANEL_MODEL_BWRY213] = {PANEL_MODEL_BWRY213, "BWRY213", 250, 122, 1, 0},
};

const panel_t *panel_find(uint8_t model)
{
    if (model == PANEL_MODEL_AUTO || model >= PANEL_MODEL_COUNT)
        return 0;
    return &panels[model];
}

uint16_t panel_plane_bytes(const panel_t *panel)
{
    return (uint16_t)((uint32_t)panel->width * ((panel->height + 7) / 8));
}
