#include <stdint.h>
#include "tl_common.h"
#include "drivers.h"
#include "application/ports/status_light.h"
#include "infrastructure/board.h"
#include "infrastructure/led.h"

// The LEDs are active low: writing 0 lights them.

// Rainbow animation state
static uint8_t rainbow_enabled = 0;
static uint16_t rainbow_hue = 0; // 0..359
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

_attribute_ram_code_ static void write_leds(uint8_t red, uint8_t green, uint8_t blue)
{
    gpio_write(LED_RED, red ? 0 : 1);
    gpio_write(LED_GREEN, green ? 0 : 1);
    gpio_write(LED_BLUE, blue ? 0 : 1);
}

_attribute_ram_code_ static void show(status_light_color_t color)
{
    switch (color)
    {
    case STATUS_LIGHT_RED:
        write_leds(1, 0, 0);
        break;
    case STATUS_LIGHT_GREEN:
        write_leds(0, 1, 0);
        break;
    case STATUS_LIGHT_BLUE:
        write_leds(0, 0, 1);
        break;
    case STATUS_LIGHT_WHITE:
        write_leds(1, 1, 1);
        break;
    default:
        write_leds(0, 0, 0);
        break;
    }
}

_attribute_ram_code_ void status_light_off(void)
{
    show(STATUS_LIGHT_OFF);
}

// Long enough to see in daylight (a 1 ms flash was hardly visible), short enough to cost little:
// a few mA for 20 ms every heartbeat.
#define BLINK_MS 20

_attribute_ram_code_ void status_light_blink(status_light_color_t color)
{
    show(color);
    WaitMs(BLINK_MS);
    show(STATUS_LIGHT_OFF);
}

void status_light_set_rainbow(uint8_t enabled)
{
    rainbow_enabled = enabled ? 1 : 0;
    if (!rainbow_enabled)
        show(STATUS_LIGHT_OFF);
    rainbow_last_step = clock_time();
    pwm_last_tick = rainbow_last_step;
    pwm_step = 0;
}

void status_light_animate(void)
{
    if (!rainbow_enabled)
        return;

    // PWM substep progression (timed ~0.5ms)
    if (clock_time_exceed(pwm_last_tick, PWM_SUBSTEP_US))
    {
        pwm_last_tick = clock_time();

        // Map targets 0..255 to duty 0..PWM_STEPS
        uint8_t duty_r = (uint16_t)pwm_target_r * PWM_STEPS / 255;
        uint8_t duty_g = (uint16_t)pwm_target_g * PWM_STEPS / 255;
        uint8_t duty_b = (uint16_t)pwm_target_b * PWM_STEPS / 255;

        write_leds(pwm_step < duty_r, pwm_step < duty_g, pwm_step < duty_b);

        pwm_step++;
        if (pwm_step >= PWM_STEPS)
            pwm_step = 0;
    }

    // Hue update every ~20ms
    if (clock_time_exceed(rainbow_last_step, 20 * 1000))
    {
        rainbow_last_step = clock_time();
        hsv_to_rgb(rainbow_hue % 360, 200, 40, &pwm_target_r, &pwm_target_g, &pwm_target_b); // moderate brightness
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
