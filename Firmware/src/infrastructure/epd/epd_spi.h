#pragma once
#include <stdint.h>
#include "infrastructure/board.h"

#define EPD_POWER_ON() gpio_write(EPD_ENABLE, 0)

#define EPD_POWER_OFF() gpio_write(EPD_ENABLE, 1)

#define EPD_ENABLE_WRITE_CMD() gpio_write(EPD_DC, 0)
#define EPD_ENABLE_WRITE_DATA() gpio_write(EPD_DC, 1)

#define EPD_IS_BUSY() (!gpio_read(EPD_BUSY))

void EPD_init(void);
void EPD_SPI_Write(unsigned char value);
uint8_t EPD_SPI_read(void);
// Sends a temperature read command (SSD16xx 0x1B, UC8151C 0x40) and returns the answer in 1/256
// degrees C. SSD16xx controllers send 12 bits (whole degrees, then 4 fraction bits in the second
// byte); the UC8151C internal sensor only whole degrees, so pass has_fraction 0 for it.
int16_t EPD_ReadTemperature(uint8_t command, uint8_t has_fraction);
void EPD_WriteCmd(unsigned char cmd);
void EPD_WriteData(unsigned char data);
void EPD_CheckStatus(int max_ms);
void EPD_CheckStatus_inverted(int max_ms);
void EPD_send_lut(const uint8_t lut[], int len);
void EPD_send_empty_lut(uint8_t lut, int len);
void EPD_WriteDataBulk(const unsigned char *data, int size);
void EPD_WriteDataRepeat(unsigned char value, int count);
void EPD_LoadImage(unsigned char *image, int size, uint8_t cmd);