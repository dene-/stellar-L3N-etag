#pragma once
#include <stdint.h>

// Interval used when the upload did not set one.
#define SLIDESHOW_DEFAULT_INTERVAL_SECONDS 60

typedef struct
{
    uint8_t index; // image currently shown
    uint32_t last_switch;
} slideshow_t;

// Starts over at the first image, shown at now.
void slideshow_restart(slideshow_t *slideshow, uint32_t now);
// Moves to the next of count images (wrapping) once interval_seconds (0 = default) have passed since
// the last switch; returns 1 when it moved. A clock set backwards restarts the wait.
uint8_t slideshow_advance(slideshow_t *slideshow, uint32_t now, uint16_t interval_seconds, uint8_t count);
