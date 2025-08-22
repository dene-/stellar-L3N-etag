#pragma once

void init_led(void);
void set_led_color(uint8_t color);

// Optional RGB control and rainbow animation
void set_led_rgb(uint8_t r, uint8_t g, uint8_t b);
void led_set_rainbow_enabled(uint8_t en);
uint8_t led_get_rainbow_enabled(void);
void led_rainbow_task(void);
