#include <stdint.h>
#include "tl_common.h"
#include "drivers.h"
#include "infrastructure/board.h"
#include "infrastructure/uart.h"

void init_uart(void)
{
	gpio_set_func(TXD, AS_GPIO);
	gpio_set_output_en(TXD, 1);
	gpio_write(TXD, 0);
	gpio_set_func(RXD, AS_GPIO);
	gpio_set_input_en(RXD, 1);
	gpio_set_output_en(RXD, 0);

	uart_gpio_set(UART_TX_PB1, UART_RX_PA0);
	uart_reset();
	uart_init(12, 15, PARITY_NONE, STOP_BIT_ONE); // baud rate: 115200
	uart_dma_enable(0, 0);
	dma_chn_irq_enable(0, 0);
	uart_irq_enable(0, 0);
	uart_ndma_irq_triglevel(0, 0);
}

// A UART that never finishes a byte must not hang the main loop: waiting for it gives up after
// UART_BYTE_TIMEOUT_US and the rest of the message is dropped. One byte takes about 87 us at 115200
// baud. A byte is only queued while the transmitter is idle, so uart_ndma_send_byte's own wait for
// FIFO space (unbounded) never starts.
#define UART_BYTE_TIMEOUT_US 1000

_attribute_ram_code_ static uint8_t wait_tx_idle(void)
{
	unsigned int start = clock_time();

	while (uart_tx_is_busy())
	{
		if (clock_time_exceed(start, UART_BYTE_TIMEOUT_US))
			return 0;
		sleep_us(10);
	}
	return 1;
}

_attribute_ram_code_ void uart_puts(const char* str)
{
	while (*str != '\0')
	{
		if (!wait_tx_idle())
			return;
		uart_ndma_send_byte(*str);
		str++;
	}
	wait_tx_idle();
}