#include "infrastructure/board.h"
#include "domain/panel.h"
#include "infrastructure/led.h"
#include "sections.h"

// Stellar L3N@, Stellar Pro 213R-N and the other tags of ATC_TLSR_Paper.
static const board_t board_stellar = {
    .epd_reset = GPIO_PD4,
    .epd_dc = GPIO_PD7,
    .epd_busy = GPIO_PA1,
    .epd_cs = GPIO_PB4,
    .epd_clk = GPIO_PB5,
    .epd_mosi = GPIO_PB6,
    .epd_enable = GPIO_PC5,
    .epd_enable_driven = 0,
    .led_red = GPIO_PD2,
    .led_green = GPIO_PD3,
    .led_blue = GPIO_PA7,
};

// The newer Hanshow boards with four-colour panels. Not verified on a Stellar 213Q-N: this is the
// layout ATC_BLE_OEPL uses for its "V2" Hanshow presets and that was probed on Nebular Pro Q and
// Nebular M3HG boards (OpenEPaperLink issue #558, atc1441.github.io issue #25). The battery
// measurement keeps PB7, which is DC here: it only drives the pin high while the panel is idle.
static const board_t board_hanshow_v2 = {
    .epd_reset = GPIO_PD4,
    .epd_dc = GPIO_PB7,
    .epd_busy = GPIO_PA1,
    .epd_cs = GPIO_PD2,
    .epd_clk = GPIO_PD7,
    .epd_mosi = GPIO_PB6,
    .epd_enable = GPIO_PB5,
    .epd_enable_driven = 1,
    .led_red = GPIO_PB4,
    .led_green = GPIO_PD3,
    .led_blue = GPIO_PA7,
};

RAM const board_t *board = &board_stellar;

void board_select_for_panel(uint8_t model)
{
    const board_t *selected = (model == PANEL_MODEL_BWRY213) ? &board_hanshow_v2 : &board_stellar;

    if (selected == board)
        return;
    board = selected;
    init_led();
}
