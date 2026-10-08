#include <stdint.h>
#include "tl_common.h"
#include "drivers.h"
#include "infrastructure/epd/epd_panel.h"
#include "domain/panel.h"
#include "domain/temperature.h"
#include "infrastructure/board.h"
#include "infrastructure/uart.h"
#include "infrastructure/epd/epd_spi.h"
#include "infrastructure/epd/epd_bw_213.h"
#include "infrastructure/epd/epd_bw_213_ice.h"
#include "infrastructure/epd/epd_bwr_154.h"
#include "infrastructure/epd/epd_bwr_213.h"
#include "infrastructure/epd/epd_bwr_296.h"
#include "infrastructure/epd/epd_bwry_213.h"

#define BUSY_START_TIMEOUT_US 20000

typedef struct
{
    uint8_t busy_active_low; // UC8151 family: BUSY low while refreshing; SSD16xx: high
    int16_t (*read_temp)(void);                                                                     // 1/256 degrees C
    int16_t (*display)(unsigned char *black, unsigned char *red, int size, uint8_t full_or_partial); // red may be NULL
    void (*sleep)(void);
} epd_driver_t;

// Index = PANEL_MODEL_* id. The black/white 2.9" panel runs on the BWR296 driver without red.
static const epd_driver_t drivers[PANEL_MODEL_COUNT] = {
    [PANEL_MODEL_BW213] = {1, EPD_BW_213_read_temp, EPD_BW_213_Display, EPD_BW_213_set_sleep},
    [PANEL_MODEL_BWR213] = {1, EPD_BWR_213_read_temp, EPD_BWR_213_Display, EPD_BWR_213_set_sleep},
    [PANEL_MODEL_BWR154] = {0, EPD_BWR_154_read_temp, EPD_BWR_154_Display, EPD_BWR_154_set_sleep},
    [PANEL_MODEL_BW213_ICE] = {0, EPD_BW_213_ice_read_temp, EPD_BW_213_ice_Display, EPD_BW_213_ice_set_sleep},
    [PANEL_MODEL_BWR296] = {0, EPD_BWR_296_read_temp, EPD_BWR_296_Display, EPD_BWR_296_set_sleep},
    [PANEL_MODEL_BW296] = {0, EPD_BWR_296_read_temp, EPD_BWR_296_Display, EPD_BWR_296_set_sleep},
    [PANEL_MODEL_BWRY213] = {1, EPD_BWRY_213_read_temp, EPD_BWRY_213_Display, EPD_BWRY_213_set_sleep},
};

void epd_panel_select(uint8_t model)
{
    board_select_for_panel(model);
}

// Powers the panel and pulses the controller's hardware reset.
_attribute_ram_code_ static void power_up_and_reset(uint16_t settle_ms)
{
    EPD_init();
    EPD_POWER_ON();
    WaitMs(settle_ms);
    gpio_write(EPD_RESET, 0);
    WaitMs(10);
    gpio_write(EPD_RESET, 1);
    WaitMs(10);
}

// Detection by controller family, read passively from the idle level of BUSY after a hardware reset:
// SSD1680/SSD16xx idle low (datasheet: BUSY high only while busy), UC8151C idles high (BUSY_N).
// No commands are sent: command probes are unsafe here, e.g. the SSD "SW reset" 0x12 starts a
// refresh on a UC8151C, and the SSD1680 has no LUT read-back. The family maps to this firmware's
// tags: SSD16xx -> 2.9" BWR296 (L3N, 290R-N), UC8151 -> 2.13" BWR213 (213R-N). Other panels must
// be selected explicitly (E0 <model>); this always runs with the Stellar wiring.
_attribute_ram_code_ uint8_t epd_panel_detect(void)
{
    uint8_t idle_high = 0;
    uint8_t model;
    uint8_t i;

    power_up_and_reset(10);
    // Majority vote over 16 ms rides out the end of the controller's reset busy phase.
    for (i = 0; i < 16; i++)
    {
        idle_high += gpio_read(EPD_BUSY) ? 1 : 0;
        WaitMs(1);
    }
    model = (idle_high > 8) ? PANEL_MODEL_BWR213 : PANEL_MODEL_BWR296;

    uart_puts("Detected :");
    uart_puts(panel_find(model)->name);
    uart_puts("\r\n");

    EPD_POWER_OFF();
    return model;
}

_attribute_ram_code_ int16_t epd_panel_read_temperature(uint8_t model)
{
    int16_t temperature;

    power_up_and_reset(5);
    temperature = drivers[model].read_temp();
    EPD_POWER_OFF();
    return temperature_x10_from_x256(temperature);
}

_attribute_ram_code_ uint8_t epd_panel_is_idle(uint8_t model)
{
    // EPD_IS_BUSY() reads BUSY as active low (UC8151); SSD16xx drive it active high.
    return drivers[model].busy_active_low ? !EPD_IS_BUSY() : EPD_IS_BUSY();
}

_attribute_ram_code_ int16_t epd_panel_refresh(uint8_t model, uint8_t *black, uint8_t *red, uint16_t size, uint8_t full)
{
    int16_t temperature;
    uint32_t start;

    power_up_and_reset(5);
    temperature = drivers[model].display(black, red, size, full);

    // The controller raises BUSY shortly after the refresh command; until it does, the panel
    // would look idle and get powered down mid-refresh. Refreshes take far longer than this.
    start = clock_time();
    while (epd_panel_is_idle(model) && !clock_time_exceed(start, BUSY_START_TIMEOUT_US))
        ;
    return temperature_x10_from_x256(temperature);
}

_attribute_ram_code_ void epd_panel_sleep(uint8_t model)
{
    drivers[model].sleep();
    EPD_POWER_OFF();
}

_attribute_ram_code_ void epd_panel_wake_on_idle(uint8_t model)
{
    // Idle level: high for UC8151 (BUSY_N), low for SSD16xx.
    cpu_set_gpio_wakeup(EPD_BUSY, drivers[model].busy_active_low ? 1 : 0, 1);
}

_attribute_ram_code_ void epd_panel_wake_on_idle_off(void)
{
    cpu_set_gpio_wakeup(EPD_BUSY, 0, 0);
}
