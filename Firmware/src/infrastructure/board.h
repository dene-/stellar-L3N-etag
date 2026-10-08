#pragma once
#include <stdint.h>
#include "tl_common.h"
#include "drivers.h"

// GPIO wiring. Hanshow uses two layouts for the panel and the LED; which one a tag has follows from
// the panel model (board_select_for_panel()).

typedef struct
{
    GPIO_PinTypeDef epd_reset;
    GPIO_PinTypeDef epd_dc;
    GPIO_PinTypeDef epd_busy;
    GPIO_PinTypeDef epd_cs;
    GPIO_PinTypeDef epd_clk;
    GPIO_PinTypeDef epd_mosi;
    GPIO_PinTypeDef epd_enable; // panel power, active low
    uint8_t epd_enable_driven;  // 0: left an input (not connected on the Stellar Pro 213R-N)
    GPIO_PinTypeDef led_red;
    GPIO_PinTypeDef led_green;
    GPIO_PinTypeDef led_blue;
} board_t;

// The layout in use; the Stellar one until board_select_for_panel() picks another.
extern const board_t *board;

// Picks the layout for a PANEL_MODEL_* id (PANEL_MODEL_AUTO: the Stellar one) and sets up the LED
// pins of the new layout.
void board_select_for_panel(uint8_t model);

#define LED_BLUE (board->led_blue)
#define LED_RED (board->led_red)
#define LED_GREEN (board->led_green)

#define EPD_RESET (board->epd_reset)
#define EPD_DC (board->epd_dc)
#define EPD_BUSY (board->epd_busy)
#define EPD_CS (board->epd_cs)
#define EPD_CLK (board->epd_clk)
#define EPD_MOSI (board->epd_mosi)
#define EPD_ENABLE (board->epd_enable)

// Shared by both layouts as far as known.
#define RXD GPIO_PA0
#define TXD GPIO_PB1

#define NFC_SDA GPIO_PC0
#define NFC_SCL GPIO_PC1
#define NFC_CS GPIO_PC6
#define NFC_IRQ GPIO_PC4
