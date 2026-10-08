#pragma once
#include <stdint.h>

// The RGB status LED (infrastructure/led).
typedef enum
{
    STATUS_LIGHT_OFF = 0,
    STATUS_LIGHT_RED,
    STATUS_LIGHT_GREEN,
    STATUS_LIGHT_BLUE,
    STATUS_LIGHT_WHITE,
} status_light_color_t;

void status_light_off(void);
// Lights color for about a millisecond.
void status_light_blink(status_light_color_t color);
// Colour-cycling animation; disabling it turns the LED off.
void status_light_set_rainbow(uint8_t enabled);
// Advances the animation if it is enabled; call on every main loop pass.
void status_light_animate(void);
