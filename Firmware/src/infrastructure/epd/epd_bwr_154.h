#pragma once

// Both return the panel temperature in 1/256 degrees C.
int16_t EPD_BWR_154_read_temp(void);
int16_t EPD_BWR_154_Display(unsigned char *image, unsigned char *red, int size, uint8_t full_or_partial); // red may be NULL
void EPD_BWR_154_set_sleep(void);