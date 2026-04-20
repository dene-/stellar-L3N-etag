#include <stdint.h>
#include "tl_common.h"
#include "app.h"
#include "main.h"
#include "drivers.h"
#include "stack/ble/ble.h"
#include "vendor/common/blt_common.h"

#include "battery.h"
#include "ble.h"
#include "cmd_parser.h"
#include "flash.h"
#include "image_store.h"
#include "ota.h"
#include "epd.h"
#include "epd_spi.h"
#include "etime.h"
#include "bart_tif.h"
#include "uart.h"

RAM uint8_t battery_level;
RAM uint16_t battery_mv;
RAM int16_t temperature;

// Settings
extern settings_struct settings;

_attribute_ram_code_ void user_init_normal(void)
{                            // this will get executed one time after power up
    random_generator_init(); // must
    init_time();
    init_ble();
    init_flash();
    image_store_init();
    init_nfc();

    // Always start on the default clock scene after a fresh boot / OTA.
    // The user can switch to slideshow mode via BLE if desired.

    // epd_display_tiff((uint8_t *)bart_tif, sizeof(bart_tif));
    // epd_display(3334533);
}

_attribute_ram_code_ void user_init_deepRetn(void)
{ // after sleep this will get executed
    blc_ll_initBasicMCU();
    rf_set_power_level_index(RF_POWER_P3p01dBm);
    blc_ll_recoverDeepRetention();
}

_attribute_ram_code_ void main_loop(void)
{
    blt_sdk_main_loop();
    handler_time();

    // Read battery & temperature less often when nobody is connected
    uint32_t sensor_interval = ble_get_connected() ? 30 : 300;
    if (time_reached_period(Timer_CH_1, sensor_interval))
    {
        battery_mv = get_battery_mv();
        battery_level = get_battery_level(battery_mv);
        temperature = EPD_read_temp();
        set_adv_data(temperature * 10, battery_level, battery_mv);
        ble_send_battery(battery_level);
        ble_send_temp(temperature * 10);
    }

    epd_update(get_time(), battery_mv, temperature);
    // LED rainbow animation (non-blocking)
    led_rainbow_task();

    if (settings.led_flashing_enabled && time_reached_period(Timer_CH_0, 10))
    {
        if (ble_get_connected())
            set_led_color(3);
        else
            set_led_color(2);
        WaitMs(1);

        set_led_color(0);
    }

    if (epd_state_handler()) // if epd_update is ongoing, sleep between BLE events and wake on EPD BUSY pin
    {
        cpu_set_gpio_wakeup(EPD_BUSY, 1, 1);
        bls_pm_setWakeupSource(PM_WAKEUP_PAD);
        bls_pm_setSuspendMask(SUSPEND_ADV | SUSPEND_CONN);
    }
    else
    {
        blt_pm_proc();
    }
}
