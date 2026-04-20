#include <stdint.h>
#include "tl_common.h"
#include "drivers.h"
#include "stack/ble/ble.h"
#include "drivers/8258/flash.h"
#include "led.h"
#include "main.h"
// Project settings (persistent)
#include "flash.h"

extern settings_struct settings;

// Simple rainbow animation state
static uint8_t rainbow_enabled = 0;
static uint16_t rainbow_hue = 0; // 0..359*1 (we'll wrap at 360)
static unsigned long rainbow_last_step = 0;

// Software PWM state for smooth fading on GPIO pins
static const uint8_t PWM_STEPS = 32;            // brightness resolution (0..31)
static const unsigned int PWM_SUBSTEP_US = 500; // 0.5ms per substep -> ~62.5Hz frame
static uint8_t pwm_step = 0;                    // 0..PWM_STEPS-1
static unsigned long pwm_last_tick = 0;
static uint8_t pwm_target_r = 0, pwm_target_g = 0, pwm_target_b = 0; // 0..255

static inline void hsv_to_rgb(uint16_t h, uint8_t s, uint8_t v, uint8_t *r, uint8_t *g, uint8_t *b)
{
    // h: 0..359, s:0..255, v:0..255
    uint8_t region = h / 60;
    uint16_t remainder = (h % 60) * 255 / 60;

    uint16_t p = (uint16_t)v * (255 - s) / 255;
    uint16_t q = (uint16_t)v * (255 - ((uint16_t)s * remainder / 255)) / 255;
    uint16_t t = (uint16_t)v * (255 - ((uint16_t)s * (255 - remainder) / 255)) / 255;

    switch (region)
    {
    default:
    case 0:
        *r = v;
        *g = (uint8_t)t;
        *b = (uint8_t)p;
        break;
    case 1:
        *r = (uint8_t)q;
        *g = v;
        *b = (uint8_t)p;
        break;
    case 2:
        *r = (uint8_t)p;
        *g = v;
        *b = (uint8_t)t;
        break;
    case 3:
        *r = (uint8_t)p;
        *g = (uint8_t)q;
        *b = v;
        break;
    case 4:
        *r = (uint8_t)t;
        *g = (uint8_t)p;
        *b = v;
        break;
    case 5:
        *r = v;
        *g = (uint8_t)p;
        *b = (uint8_t)q;
        break;
    }
}

void set_led_rgb(uint8_t r, uint8_t g, uint8_t b)
{
    // Respect global LED disable setting
    if (!settings.led_flashing_enabled)
    {
        gpio_write(LED_RED, 1);
        gpio_write(LED_GREEN, 1);
        gpio_write(LED_BLUE, 1);
        return;
    }
    if (rainbow_enabled)
    {
        // When rainbow is enabled, store targets; PWM task renders smoothly
        pwm_target_r = r;
        pwm_target_g = g;
        pwm_target_b = b;
        return;
    }
    // Otherwise do immediate binary set (legacy behavior)
    gpio_write(LED_RED, r ? 0 : 1);
    gpio_write(LED_GREEN, g ? 0 : 1);
    gpio_write(LED_BLUE, b ? 0 : 1);
}

void led_set_rainbow_enabled(uint8_t en)
{
    rainbow_enabled = en ? 1 : 0;
    if (!rainbow_enabled)
    {
        // turn off LEDs when disabling
        gpio_write(LED_BLUE, 1);
        gpio_write(LED_RED, 1);
        gpio_write(LED_GREEN, 1);
    }
    rainbow_last_step = clock_time();
    pwm_last_tick = rainbow_last_step;
    pwm_step = 0;
}

uint8_t led_get_rainbow_enabled(void)
{
    return rainbow_enabled;
}

// Call this frequently from main_loop() to animate without blocking
void led_rainbow_task(void)
{
    if (!rainbow_enabled)
        return;

    // Respect global LED disable setting
    if (!settings.led_flashing_enabled)
    {
        // Ensure off while disabled
        gpio_write(LED_RED, 1);
        gpio_write(LED_GREEN, 1);
        gpio_write(LED_BLUE, 1);
        return;
    }

    // PWM substep progression (timed ~0.5ms)
    if (clock_time_exceed(pwm_last_tick, PWM_SUBSTEP_US))
    {
        pwm_last_tick = clock_time();

        // Map targets 0..255 to duty 0..PWM_STEPS
        uint8_t duty_r = (uint16_t)pwm_target_r * PWM_STEPS / 255;
        uint8_t duty_g = (uint16_t)pwm_target_g * PWM_STEPS / 255;
        uint8_t duty_b = (uint16_t)pwm_target_b * PWM_STEPS / 255;

        // Active-low: 0 = on
        gpio_write(LED_RED, (pwm_step < duty_r) ? 0 : 1);
        gpio_write(LED_GREEN, (pwm_step < duty_g) ? 0 : 1);
        gpio_write(LED_BLUE, (pwm_step < duty_b) ? 0 : 1);

        pwm_step++;
        if (pwm_step >= PWM_STEPS)
            pwm_step = 0;
    }

    // Hue update every ~20ms
    if (clock_time_exceed(rainbow_last_step, 20 * 1000))
    {
        rainbow_last_step = clock_time();
        uint8_t r, g, b;
        hsv_to_rgb(rainbow_hue % 360, 200, 40, &r, &g, &b); // moderate brightness
        // Store to PWM targets
        pwm_target_r = r;
        pwm_target_g = g;
        pwm_target_b = b;

        rainbow_hue = (rainbow_hue + 3) % 360; // slower hue for smoothness
    }
}

_attribute_ram_code_ void init_led(void)
{
    gpio_setup_up_down_resistor(LED_BLUE, PM_PIN_PULLUP_1M);
    gpio_write(LED_BLUE, 1);
    gpio_set_func(LED_BLUE, AS_GPIO);
    gpio_set_output_en(LED_BLUE, 1);
    gpio_set_input_en(LED_BLUE, 0);

    gpio_write(LED_RED, 1);
    gpio_setup_up_down_resistor(LED_RED, PM_PIN_PULLUP_1M);
    gpio_set_func(LED_RED, AS_GPIO);
    gpio_set_output_en(LED_RED, 1);
    gpio_set_input_en(LED_RED, 0);

    gpio_setup_up_down_resistor(LED_GREEN, PM_PIN_PULLUP_1M);
    gpio_write(LED_GREEN, 1);
    gpio_set_func(LED_GREEN, AS_GPIO);
    gpio_set_output_en(LED_GREEN, 1);
    gpio_set_input_en(LED_GREEN, 0);
}

_attribute_ram_code_ void set_led_color(uint8_t color)
{
    // Respect global LED disable setting everywhere (including EPD updates)
    if (!settings.led_flashing_enabled)
    {
        gpio_write(LED_BLUE, 1);
        gpio_write(LED_RED, 1);
        gpio_write(LED_GREEN, 1);
        return;
    }
    switch (color)
    {
    case 1:
        gpio_write(LED_BLUE, 1);
        gpio_write(LED_RED, 0);
        gpio_write(LED_GREEN, 1);
        break;
    case 2:
        gpio_write(LED_BLUE, 1);
        gpio_write(LED_RED, 1);
        gpio_write(LED_GREEN, 0);
        break;
    case 3:
        gpio_write(LED_BLUE, 0);
        gpio_write(LED_RED, 1);
        gpio_write(LED_GREEN, 1);
        break;
    case 4:
        gpio_write(LED_BLUE, 0);
        gpio_write(LED_RED, 0);
        gpio_write(LED_GREEN, 0);
        break;
    default:
        gpio_write(LED_BLUE, 1);
        gpio_write(LED_RED, 1);
        gpio_write(LED_GREEN, 1);
        break;
    }
}
