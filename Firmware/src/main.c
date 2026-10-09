// Composition root: boots the hardware, wires the BLE services to the use cases and runs the main loop.
#include <stdint.h>
#include "tl_common.h"
#include "drivers.h"
#include "stack/ble/ble.h"
#include "vendor/common/blt_common.h"
#include "application/device_settings.h"
#include "application/local_time.h"
#include "application/display.h"
#include "application/power.h"
#include "application/screen.h"
#include "application/status_led.h"
#include "application/telemetry.h"
#include "ble/ble.h"
#include "infrastructure/board.h"
#include "infrastructure/epd/epd_panel.h"
#include "infrastructure/i2c.h"
#include "infrastructure/led.h"
#include "infrastructure/nfc.h"
#include "infrastructure/uart.h"
#include "infrastructure/storage/image_store.h"
#include "infrastructure/wall_clock.h"
#include "sections.h"

// Panel model whose BUSY idle level is armed as a wake-up source; PANEL_MODEL_AUTO = none.
static RAM uint8_t busy_wakeup_model = PANEL_MODEL_AUTO;

_attribute_ram_code_ __attribute__((optimize("-Os"))) void irq_handler(void)
{
	irq_blt_sdk_handler();
}

// Resets the chip if the main loop does not run for this long. Every iteration is far shorter than
// this: the loop runs at least once per advertising interval (1 s) or connection event (250 ms
// requested, a phone may pick up to 4 s), and the longest work inside one is starting a panel
// refresh, a few BUSY waits of at most 500 ms each. Flash erases clear the watchdog themselves
// (SDK flash.c) and the OTA copy stops it. Even if it kept counting in suspend, a sleep ends at the
// next advertising or connection event, so it cannot trip there. Deep retention resets the chip's
// registers, so main() arms it again on every wake-up.
#define WATCHDOG_MS 8000

// Runs once after power up (not after deep-retention wake-ups).
_attribute_ram_code_ static void init_normal(void)
{
	random_generator_init(); // must
	wall_clock_init();
	// Sampled here, before anything can start a panel refresh, so a dying cell does not brown out on
	// the boot refresh again and again.
	power_sample_battery();
	ble_init();
	image_store_init(); // before the settings: older settings take the slideshow interval from it
	device_settings_load();
	local_time_init();
	display_init(device_settings_panel_model());
	screen_restore_scene();
	init_nfc();
}

// Runs after every deep-retention wake-up; RAM-retained state is still valid.
_attribute_ram_code_ static void init_after_deep_retention(void)
{
	blc_ll_initBasicMCU();
	rf_set_power_level_index(RF_POWER_P3p01dBm);
	blc_ll_recoverDeepRetention();
}

_attribute_ram_code_ static void main_loop(void)
{
	uint8_t connected;

	wd_clear();
	blt_sdk_main_loop();
	wall_clock_tick();

	connected = ble_is_connected();
	telemetry_update(connected);
	screen_update(connected, ble_device_name());
	status_led_update(connected);
	// Settings changed over BLE are stored once the link is closed, never while a panel refresh runs.
	if (!connected && !display_is_refreshing())
		device_settings_save_if_changed();

	if (display_poll())
	{ // a refresh is running: sleep between BLE events only, and wake when the panel's BUSY pin goes idle
		uint8_t model = display_panel()->model;

		if (busy_wakeup_model != model) // (re)arm for this panel's idle level
		{
			epd_panel_wake_on_idle(model);
			busy_wakeup_model = model;
		}
		bls_pm_setWakeupSource(PM_WAKEUP_PAD);
		bls_pm_setSuspendMask(SUSPEND_ADV | SUSPEND_CONN);
	}
	else
	{
		if (busy_wakeup_model != PANEL_MODEL_AUTO)
		{
			epd_panel_wake_on_idle_off();
			busy_wakeup_model = PANEL_MODEL_AUTO;
		}
		bls_pm_setSuspendMask(SUSPEND_ADV | DEEPSLEEP_RETENTION_ADV | SUSPEND_CONN | DEEPSLEEP_RETENTION_CONN);
	}
}

_attribute_ram_code_ int main(void) // must run in ramcode
{
	blc_pm_select_internal_32k_crystal();
	cpu_wakeup_init();
	int deepRetWakeUp = pm_is_MCU_deepRetentionWakeup(); // MCU deep retention wakeUp
	rf_drv_init(RF_MODE_BLE_1M);
	gpio_init(!deepRetWakeUp); // analog resistance will keep available in deepSleep mode, so no need initialize again
#if (CLOCK_SYS_CLOCK_HZ == 16000000)
	clock_init(SYS_CLK_16M_Crystal);
#elif (CLOCK_SYS_CLOCK_HZ == 24000000)
	clock_init(SYS_CLK_24M_Crystal);
#endif
	wd_set_interval_ms(WATCHDOG_MS, CLOCK_SYS_CLOCK_1MS);
	wd_start();
	if (!deepRetWakeUp)
		image_store_repair_legacy_overlap();
	blc_app_loadCustomizedParameters();

	init_led();
	init_uart();
	init_i2c();

	if (deepRetWakeUp)
	{
		init_after_deep_retention();
		uart_puts("--- Wake from deep\r\n");
	}
	else
	{
		uart_puts("\r\n\r\n --- Booting normal--- \r\n");
		init_normal();
	}
	irq_enable();
	while (1)
	{
		main_loop();
	}
}
