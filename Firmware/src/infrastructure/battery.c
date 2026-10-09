#include <stdint.h>
#include "tl_common.h"
#include "drivers.h"
#include "application/ports/battery_sensor.h"

_attribute_ram_code_ uint16_t battery_sensor_read_mv(void)
{
	uint16_t mv;

	adc_init();
	adc_vbat_init(GPIO_PB7);
	adc_power_on_sar_adc(1);
	mv = adc_sample_and_get_result();
	adc_power_on_sar_adc(0); // the SAR ADC draws current for as long as it is powered
	return mv;
}
