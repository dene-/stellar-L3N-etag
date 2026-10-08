#include "domain/slideshow.h"

void slideshow_restart(slideshow_t *slideshow, uint32_t now)
{
    slideshow->index = 0;
    slideshow->last_switch = now;
}

uint8_t slideshow_advance(slideshow_t *slideshow, uint32_t now, uint16_t interval_seconds, uint8_t count)
{
    if (!interval_seconds)
        interval_seconds = SLIDESHOW_DEFAULT_INTERVAL_SECONDS;
    if (now < slideshow->last_switch)
        slideshow->last_switch = now;
    if (!count || now - slideshow->last_switch < interval_seconds)
        return 0;

    slideshow->last_switch = now;
    slideshow->index = (slideshow->index + 1) % count;
    return 1;
}
