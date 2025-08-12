#include <stdint.h>
#include "tl_common.h"
#include "main.h"
#include "epd.h"
#include "epd_spi.h"
#include "epd_bwr_213.h"
#include "drivers.h"
#include "stack/ble/ble.h"
#include "led.h"

// UC8151C or similar EPD Controller

enum PSR_FLAGS
{
    RES_96x230 = 0b00000000,
    RES_96x252 = 0b01000000,
    RES_128x296 = 0b10000000,
    RES_160x296 = 0b11000000,

    LUT_OTP = 0b00000000,
    LUT_REG = 0b00100000,

    FORMAT_BWR = 0b00000000,
    FORMAT_BW = 0b00010000,

    SCAN_DOWN = 0b00000000,
    SCAN_UP = 0b00001000,

    SHIFT_LEFT = 0b00000000,
    SHIFT_RIGHT = 0b00000100,

    BOOSTER_OFF = 0b00000000,
    BOOSTER_ON = 0b00000010,

    RESET_SOFT = 0b00000000,
    RESET_NONE = 0b00000001
};

#define lut_bwr_213_refresh_time 10
uint8_t lut_bwr_213_20_part[] =
    {
        0x20, 0x00, lut_bwr_213_refresh_time, 0x01, 0x00, 0x00, 0x01, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00};
uint8_t lut_bwr_213_22_part[] =
    {
        0x22, 0x80, lut_bwr_213_refresh_time, 0x01, 0x00, 0x00, 0x01, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00};
uint8_t lut_bwr_213_23_part[] =
    {
        0x23, 0x40, lut_bwr_213_refresh_time, 0x01, 0x00, 0x00, 0x01, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00};


#define EPD_BWR_213_test_pattern 0xA5
_attribute_ram_code_ uint8_t EPD_BWR_213_detect(void)
{
    // SW Reset
    EPD_WriteCmd(0x12);
    WaitMs(10);

    EPD_WriteCmd(0x32);
    int i;
    for (i = 0; i < 153; i++)// This model has a 159 bytes LUT storage so we test for that
    {
        EPD_WriteData(EPD_BWR_213_test_pattern);
    }
    EPD_WriteCmd(0x33);
    for (i = 0; i < 153; i++)
    {
        if(EPD_SPI_read() != EPD_BWR_213_test_pattern) 
            return 0;
    }
    return 1;
}

_attribute_ram_code_ uint8_t EPD_BWR_213_read_temp(void)
{
    uint8_t epd_temperature = 0 ;

    EPD_WriteCmd(0x04);

    WaitMs(1);

    // Temperature sensor read from register
    EPD_WriteCmd(0x40);
    epd_temperature = EPD_SPI_read();    
    epd_temperature = epd_temperature * 256 + EPD_SPI_read();

    // WaitMs(5);
    
    // power off
    EPD_WriteCmd(0x02);

    // deep sleep
    EPD_WriteCmd(0x07);
    EPD_WriteData(0xa5);

    return epd_temperature;
}

#define scan_direction (SCAN_UP | RES_128x296 | FORMAT_BWR | BOOSTER_ON | RESET_NONE | LUT_OTP | SHIFT_RIGHT)
uint8_t EPD_BWR_213_Display_start(uint8_t full_or_partial)
{
    uint8_t epd_temperature = 0;

    // power on
    EPD_WriteCmd(0x04);

    WaitMs(1);

    /*EPD_WriteCmd(0X4D);
    EPD_WriteData(0x55);
    EPD_WriteCmd(0XF3);
    EPD_WriteData(0x0A);
    EPD_WriteCmd(0X50);
    EPD_WriteData(0x57);*/

    EPD_WriteCmd(0x00);
    EPD_WriteData(scan_direction);
    EPD_WriteData(0x0f);

    EPD_WriteCmd(0x40);
    epd_temperature = EPD_SPI_read();
    EPD_SPI_read();

    EPD_WriteCmd(0x10);

    return epd_temperature;
}
void EPD_BWR_213_Display_byte(uint8_t data)
{
    EPD_WriteData(data);
}
void EPD_BWR_213_Display_buffer(unsigned char *image, int size)
{
    for (int i = 0; i < size; i++)
    {
        EPD_WriteData(image[i]);
    }
}

void EPD_BWR_213_Display_color_change()
{
    EPD_WriteCmd(0x13);
}

void EPD_BWR_213_Display_end()
{
    EPD_WriteCmd(0x12);
}

_attribute_ram_code_ uint8_t EPD_BWR_213_Display(unsigned char *image, int size, uint8_t full_or_partial)
{    
    uint8_t epd_temperature = 0 ;
    
    // power on
    EPD_WriteCmd(0x04);
    WaitMs(1);

    EPD_WriteCmd(0x00);
    EPD_WriteData(scan_direction); //| LUT_REG);
    EPD_WriteData(0x0f);

    EPD_WriteCmd(0x40);
    epd_temperature = EPD_SPI_read();
    EPD_SPI_read();

    /*EPD_send_lut(lut_bwr_213_20_part, sizeof(lut_bwr_213_20_part));
    EPD_send_empty_lut(0x21, 260);
    EPD_send_lut(lut_bwr_213_22_part, sizeof(lut_bwr_213_22_part));
    EPD_send_lut(lut_bwr_213_23_part, sizeof(lut_bwr_213_23_part));
    EPD_send_empty_lut(0x24, 260);*/

    set_led_color(2);
        WaitMs(5);
    set_led_color(0);
        WaitMs(5);
    set_led_color(2);
        WaitMs(5);
     set_led_color(0);
        WaitMs(5);       
    //////////////////////// This parts clears the full screen
    EPD_WriteCmd(0x10);
    int i;
    for (i = 0; i < 8832; i++)
    {
        EPD_WriteData(0);
    }

    set_led_color(1);
        WaitMs(5);
    set_led_color(0);
        WaitMs(5);
    set_led_color(1);
        WaitMs(5);
     set_led_color(0);
        WaitMs(5);       

    EPD_WriteCmd(0x13);         // Display_color_change()
    for (i = 0; i < 8832; i++)
    {
        EPD_WriteData(0);
    }

    set_led_color(3);
        WaitMs(5);
    set_led_color(0);
        WaitMs(5);
    set_led_color(3);
        WaitMs(5);
     set_led_color(0);
        WaitMs(5);       
    // epd_LoadImage
    EPD_WriteCmd(0x10);// BLACK Color

    int redpos = size /2;
    for (int i = 0; i < size ; i++)
    {
        EPD_WriteData(image[i]);
    }

    /*EPD_WriteCmd(0x13);// RED Color 
    for (i = 0; i < redpos; i++)
    {
        EPD_WriteData(~image[redpos+i]);
    }
    */
    /*if (!full_or_partial)
    {
        EPD_WriteCmd(0x32);
        for (i = 0; i < sizeof(LUT_bwr_213_part); i++)
        {
            EPD_WriteData(LUT_bwr_213_part[i]);
        }
    }
    */

    //  trigger display refresh
    EPD_WriteCmd(0x12);

    return epd_temperature;
}

_attribute_ram_code_ uint8_t EPD_BWR_213_Display_BWR(unsigned char *image, unsigned char *redimage,int size, uint8_t full_or_partial)
{    
    uint8_t epd_temperature = 0 ;
    
    // power on
    EPD_WriteCmd(0x04);
    WaitMs(1);

    EPD_WriteCmd(0x00);
    EPD_WriteData(scan_direction); //| LUT_REG);
    EPD_WriteData(0x0f);

    EPD_WriteCmd(0x40);
    epd_temperature = EPD_SPI_read();
    epd_temperature = epd_temperature * 256 + EPD_SPI_read();

    /*EPD_send_lut(lut_bwr_213_20_part, sizeof(lut_bwr_213_20_part));
    EPD_send_empty_lut(0x21, 260);
    EPD_send_lut(lut_bwr_213_22_part, sizeof(lut_bwr_213_22_part));
    EPD_send_lut(lut_bwr_213_23_part, sizeof(lut_bwr_213_23_part));
    EPD_send_empty_lut(0x24, 260);*/

    //////////////////////// This parts clears the full screen
    set_led_color(1);
    
    
    EPD_WriteCmd(0x10);
    int i;
    for (i = 0; i < 4000; i++)
    {
        EPD_WriteData(0);
    }

    EPD_WriteCmd(0x13);         // Display_color_change()
    for (i = 0; i < 4000; i++)
    {
        EPD_WriteData(0);
    }
    
    WaitMs(5);

    set_led_color(4);

    if (image != NULL) {
        EPD_WriteCmd(0x10);// BLACK Color start Data
        for (int i = 0; i < size ; i++)
        {
            EPD_WriteData(image[i]);
        }
    }
    if (redimage != NULL) {
        EPD_WriteCmd(0x13);// RED Color start Data
        for (int i = 0; i < size; i++)
        {
            EPD_WriteData(redimage[i]);
        }
    }
    /*if (!full_or_partial)
    {
        EPD_WriteCmd(0x32);
        for (i = 0; i < sizeof(LUT_bwr_213_part); i++)
        {
            EPD_WriteData(LUT_bwr_213_part[i]);
        }
    }
    */
    //  trigger display refresh
    set_led_color(1); 
    EPD_WriteCmd(0x12);

    return epd_temperature;
}

_attribute_ram_code_ void EPD_BWR_213_set_sleep(void)
{
    // power off
    EPD_WriteCmd(0x02);

    // deep sleep
    EPD_WriteCmd(0x07);
    EPD_WriteData(0xa5);

}