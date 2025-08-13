#include <stdint.h>
#include "tl_common.h"
#include "main.h"
#include "epd.h"
#include "epd_spi.h"
#include "drivers.h"
#include "stack/ble/ble.h"

_attribute_ram_code_ void EPD_init(void)
{
    gpio_set_func(EPD_RESET, AS_GPIO);
    gpio_set_output_en(EPD_RESET, 1);
    gpio_setup_up_down_resistor(EPD_RESET, PM_PIN_PULLUP_1M);

    gpio_set_func(EPD_DC, AS_GPIO);
    gpio_set_output_en(EPD_DC, 1);
    gpio_setup_up_down_resistor(EPD_DC, PM_PIN_PULLUP_1M);

    gpio_set_func(EPD_BUSY, AS_GPIO);
    gpio_set_output_en(EPD_BUSY, 0);
    gpio_set_input_en(EPD_BUSY, 1);
    gpio_setup_up_down_resistor(EPD_BUSY, PM_PIN_PULLUP_1M);

    gpio_set_func(EPD_CS, AS_GPIO);
    gpio_set_output_en(EPD_CS, 1);
    gpio_setup_up_down_resistor(EPD_CS, PM_PIN_PULLUP_1M);

    gpio_set_func(EPD_CLK, AS_GPIO);
    gpio_set_output_en(EPD_CLK, 1);
    gpio_setup_up_down_resistor(EPD_CLK, PM_PIN_PULLUP_1M);

    gpio_set_func(EPD_MOSI, AS_GPIO);
    gpio_set_output_en(EPD_MOSI, 1);
    gpio_setup_up_down_resistor(EPD_MOSI, PM_PIN_PULLUP_1M);

    gpio_set_output_en(EPD_ENABLE, 0);
    gpio_set_input_en(EPD_ENABLE, 1);
    gpio_setup_up_down_resistor(EPD_ENABLE, PM_PIN_PULLUP_1M);
}

// Tunable minimal inter-byte delay (us). 0 = fastest. Adjust if signal integrity issues.
static unsigned char epd_spi_byte_delay_us = 0;

_attribute_ram_code_ void EPD_SPI_Write(unsigned char value)
{
    unsigned char i;
    if (epd_spi_byte_delay_us)
        WaitUs(epd_spi_byte_delay_us);
    for (i = 0; i < 8; i++)
    {
        gpio_write(EPD_CLK, 0);
        gpio_write(EPD_MOSI, (value & 0x80) ? 1 : 0);
        value <<= 1;
        gpio_write(EPD_CLK, 1);
    }
}

// Burst write a buffer as DATA with CS low for the whole transfer (reduces per-byte overhead)
_attribute_ram_code_ void EPD_WriteDataStream(const unsigned char *data, int len)
{
    int i;
    gpio_write(EPD_CS, 0);
    EPD_ENABLE_WRITE_DATA();
    for (i = 0; i < len; i++)
    {
        EPD_SPI_Write(data[i]);
    }
    gpio_write(EPD_CS, 1);
}

_attribute_ram_code_ uint8_t EPD_SPI_read(void)
{
    unsigned char i;
    uint8_t value = 0;

    gpio_shutdown(EPD_MOSI);
    gpio_set_output_en(EPD_MOSI, 0);
    gpio_set_input_en(EPD_MOSI, 1);
    gpio_write(EPD_CS, 0);
    EPD_ENABLE_WRITE_DATA();
    WaitUs(10);
    for (i = 0; i < 8; i++)
    {
        gpio_write(EPD_CLK, 0);
        gpio_write(EPD_CLK, 1);
        value <<= 1;
        if (gpio_read(EPD_MOSI) != 0)
        {
            value |= 1;
        }
    }
    gpio_set_output_en(EPD_MOSI, 1);
    gpio_set_input_en(EPD_MOSI, 0);
    gpio_write(EPD_CS, 1);
    return value;
}

_attribute_ram_code_ void EPD_WriteCmd(unsigned char cmd)
{
    gpio_write(EPD_CS, 0);
    EPD_ENABLE_WRITE_CMD();
    EPD_SPI_Write(cmd);
    gpio_write(EPD_CS, 1);
}

_attribute_ram_code_ void EPD_WriteData(unsigned char data)
{
    gpio_write(EPD_CS, 0);
    EPD_ENABLE_WRITE_DATA();
    EPD_SPI_Write(data);
    gpio_write(EPD_CS, 1);
}

// Write the same byte value len times with CS held low
_attribute_ram_code_ void EPD_WriteDataRepeat(unsigned char value, int len)
{
    int i;
    gpio_write(EPD_CS, 0);
    EPD_ENABLE_WRITE_DATA();
    for (i = 0; i < len; i++)
    {
        EPD_SPI_Write(value);
    }
    gpio_write(EPD_CS, 1);
}

_attribute_ram_code_ void EPD_CheckStatus(int max_ms)
{
    unsigned long timeout_start = clock_time();
    unsigned long timeout_ticks = max_ms * CLOCK_16M_SYS_TIMER_CLK_1MS;
    WaitMs(1);
    while (EPD_IS_BUSY())
    {
        if (clock_time() - timeout_start >= timeout_ticks)
        {
            puts("Busy timeout\r\n");
            return; // Here we had a timeout
        }
    }
}

_attribute_ram_code_ void EPD_CheckStatus_inverted(int max_ms)
{
    unsigned long timeout_start = clock_time();
    unsigned long timeout_ticks = max_ms * CLOCK_16M_SYS_TIMER_CLK_1MS;
    WaitMs(1);
    while (!EPD_IS_BUSY())
    {
        if (clock_time() - timeout_start >= timeout_ticks)
            return; // Here we had a timeout
    }
}

_attribute_ram_code_ void EPD_send_lut(uint8_t lut[], int len)
{
    EPD_WriteCmd(lut[0]);
    for (int r = 1; r <= len; r++)
    {
        EPD_WriteData(lut[r]);
    }
}

_attribute_ram_code_ void EPD_send_empty_lut(uint8_t lut, int len)
{
    EPD_WriteCmd(lut);
    for (int r = 0; r <= len; r++)
        EPD_WriteData(0x00);
}

_attribute_ram_code_ void EPD_LoadImage(unsigned char *image, int size, uint8_t cmd)
{
    EPD_WriteCmd(cmd);
    EPD_WriteDataStream(image, size);
    // Removed idle delay; rely on BUSY where required
}