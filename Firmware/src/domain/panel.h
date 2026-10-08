#pragma once
#include <stdint.h>

// E-paper panel models. The ids are part of the BLE protocol (command E0 <model>, reported by E2 AB)
// and are persisted in the settings, so never renumber them.
enum
{
    PANEL_MODEL_AUTO = 0, // detect the controller family on first use
    PANEL_MODEL_BW213 = 1,
    PANEL_MODEL_BWR213 = 2,
    PANEL_MODEL_BWR154 = 3,
    PANEL_MODEL_BW213_ICE = 4,
    PANEL_MODEL_BWR296 = 5,
    PANEL_MODEL_BW296 = 6,
    PANEL_MODEL_BWRY213 = 7, // four colours, newer Hanshow board wiring (Stellar 213Q-N); experimental
    PANEL_MODEL_COUNT
};

// Largest plane of any panel: 200*200/8 (BWR154).
#define PANEL_MAX_PLANE_BYTES 5000

typedef struct
{
    uint8_t model;
    const char *name;
    uint16_t width;
    uint16_t height; // visible rows; a column takes (height+7)/8 bytes, see domain/epd_canvas.h
    // Colours beyond black and white, in the two planes: a pixel set only in the red plane is red.
    // On PANEL_MODEL_BWRY213 a pixel set in both planes is yellow (see domain/bwry.h); BWR panels
    // show it red.
    uint8_t has_red;
    uint8_t has_partial; // partial refreshes, used between full ones on the clock screens
} panel_t;

// NULL for PANEL_MODEL_AUTO and unknown ids.
const panel_t *panel_find(uint8_t model);
// Bytes in one bit plane (black or red) of the panel: width columns of (height+7)/8 bytes.
uint16_t panel_plane_bytes(const panel_t *panel);
