#pragma once
#include "etime.h"
#define epd_height 128
#define epd_width 250
#define epd_buffer_size 4736 // max buffer size: 296*128/8 = 4736

// Scene ids are part of the BLE protocol (command E1 <scene>).
enum
{
    EPD_SCENE_IMAGE = 0,     // last uploaded image
    EPD_SCENE_CLOCK = 1,     // large clock
    EPD_SCENE_DASHBOARD = 2, // clock with tag name, temperature and battery (default)
    EPD_SCENE_SLIDESHOW = 3, // cycle through uploaded images
};

// Display drivers (command E0 <model>, reported by E2 AB).
enum
{
    EPD_MODEL_AUTO = 0, // detect on first use; unreliable, see EPD_detect_model()
    EPD_MODEL_BW213 = 1,
    EPD_MODEL_BWR213 = 2,
    EPD_MODEL_BWR154 = 3,
    EPD_MODEL_BW213_ICE = 4,
    EPD_MODEL_BWR296 = 5,
};

// This firmware targets the Stellar L3N@ 2.9" tag (SSD1680, which auto-detection cannot identify).
#define EPD_DEFAULT_MODEL EPD_MODEL_BWR296

void set_EPD_model(uint8_t model_nr);
uint8_t get_EPD_model(void);
void set_EPD_scene(uint8_t scene);
void set_EPD_wait_flush();
void set_EPD_fast_refresh_enabled(uint8_t enabled);
uint8_t get_EPD_fast_refresh_enabled(void);
uint8_t get_EPD_fast_refresh_supported(void);
void EPD_detect_model(void);

void epd_get_resolution(uint8_t model_nr, uint16_t *width, uint16_t *height);
void epd_get_current_resolution(uint16_t *width, uint16_t *height);
uint16_t epd_get_buffer_size_for_model(uint8_t model_nr);
uint16_t epd_get_current_buffer_size(void);

int8_t EPD_read_temp(void);

void EPD_Display(unsigned char *image, unsigned char *red_image, int size, uint8_t full_or_partial);
void epd_display_tiff(uint8_t *pData, int iSize);
void epd_set_sleep(void);
uint8_t epd_state_handler(void);
void epd_display_char(uint8_t data);
void epd_clear(void);

void epd_update(struct date_time _time, uint16_t battery_mv, int16_t temperature);
