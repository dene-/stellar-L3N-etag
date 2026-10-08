#include <stdint.h>
#include "tl_common.h"
#include "drivers.h"
#include "infrastructure/i2c.h"
#include "sections.h"

RAM bool i2c_sending;

void init_i2c(void)
{
	i2c_gpio_set(I2C_GPIO_GROUP_C0C1);
	i2c_master_init(0x78, (uint8_t)(CLOCK_SYS_CLOCK_HZ / (4 * 400000)));
}

void send_i2c(uint8_t device_id, uint8_t *buffer, int dataLen)
{
	if (i2c_sending)
		return;
	i2c_sending = true;
	i2c_set_id(device_id);
	i2c_write_series(0, 0, (uint8_t *)buffer, dataLen);
	i2c_sending = false;
}