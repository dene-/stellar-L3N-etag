#pragma once

uint8_t EPD_BW_213_ice_read_temp(void);
uint8_t EPD_BW_213_ice_Display(unsigned char *image, unsigned char *red, int size, uint8_t full_or_partial); // red ignored
void EPD_BW_213_ice_set_sleep(void);