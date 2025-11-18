#include "Splash.h"
#include "Globals.h"
#include "led_strip.h"
#include "Blocks.h"
#include "Controls.h"
#include <stdlib.h>
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"

// splash_design_map: 24 rows x 16 cols; each value: 0 = transparent, 1..7 -> block color index
// User can edit this to create custom designs
static const uint8_t splash_design_map[24][16] = {
    {0,0,0,0, 0,0,0,0, 0,0,0,0, 0,0,0,0},
    {0,0,0,0, 0,0,0,0, 0,0,0,0, 0,0,0,0},
    {0,0,0,0, 0,0,0,0, 0,0,0,0, 0,0,0,0},
    {0,0,0,0, 0,0,0,0, 0,0,0,0, 0,0,0,0},
    {0,0,0,0, 0,0,0,0, 0,0,0,0, 0,0,0,0},
    {0,0,0,0, 0,0,0,0, 0,0,0,0, 0,0,0,0},
    {0,0,0,0, 0,0,0,0, 0,0,0,0, 0,0,0,0},
    {0,0,0,0, 0,0,0,0, 0,0,0,0, 0,0,0,0},
    {0,0,0,0, 0,0,0,0, 0,0,0,0, 0,0,0,0},
    {0,0,0,0, 0,0,0,0, 0,0,0,0, 0,0,0,0},
    {0,0,0,0, 0,0,0,0, 0,0,0,0, 0,0,0,0},
    {0,0,0,0, 0,0,0,0, 0,0,0,0, 0,0,0,0},
    {0,0,0,0, 0,0,0,0, 0,0,0,0, 0,0,0,0},
    {0,0,0,0, 0,0,0,0, 0,0,0,0, 0,0,0,0},
    {0,0,0,0, 0,0,0,0, 1,1,0,0, 0,0,0,0},
    {0,0,0,0, 0,0,0,0, 1,1,0,0, 0,0,0,0},
    {0,0,4,4, 0,0,0,0, 1,1,0,0, 0,0,0,0},
    {0,0,4,4, 0,0,0,0, 1,1,0,0, 0,0,0,0},
    {0,0,4,4, 4,4,7,7, 1,1,6,6, 6,6,0,0},
    {0,0,4,4, 4,4,7,7, 1,1,6,6, 6,6,0,0},
    {0,0,5,5, 4,4,7,7, 1,1,2,2, 6,6,6,6},
    {0,0,5,5, 4,4,7,7, 1,1,2,2, 6,6,6,6},
    {5,5,5,5, 5,5,7,7, 7,7,2,2, 2,2,2,2},
    {5,5,5,5, 5,5,7,7, 7,7,2,2, 2,2,2,2}
};

void splash_show(uint32_t duration_ms) {
    // Build the text bitmap (3x5 font) for "TETRIS"
    static const uint8_t ch_T[3] = {1, 31, 1};
    static const uint8_t ch_E[3] = {31, 21, 21};
    static const uint8_t ch_R[3] = {31, 5, 26};
    static const uint8_t ch_I[3] = {17, 31, 17};
    static const uint8_t ch_S[3] = {18, 21, 9};

    const char *txt = "TETRIS";
    const int char_w = 3;
    const int spacing = 1;
    int len = 6;
    int total_cols = len * (char_w + spacing);

    uint8_t *cols = malloc(total_cols);
    if (!cols) return;
    int pos = 0;
    for (int i = 0; i < len; i++){
        const uint8_t *bmp = NULL;
        switch (txt[i]){
            case 'T': bmp = ch_T; break;
            case 'E': bmp = ch_E; break;
            case 'R': bmp = ch_R; break;
            case 'I': bmp = ch_I; break;
            case 'S': bmp = ch_S; break;
            default: bmp = NULL; break;
        }
        for (int c = 0; c < char_w; c++){
            cols[pos++] = bmp ? (bmp[c] & 31) : 0;
        }
        if (spacing) cols[pos++] = 0;
    }

    // Render design map once (static background)
    for (int y = 0; y < LED_HEIGHT; y++) {
        for (int x = 0; x < LED_WIDTH; x++) {
            uint8_t val = splash_design_map[y][x];
            if (val == 0) continue;
            uint8_t bidx = (val - 1) % NUM_BLOCKS;
            uint8_t r, g, b;
            get_block_rgb(bidx, &r, &g, &b);
            r = (r * GAME_BRIGHTNESS_SCALE) / 255;
            g = (g * GAME_BRIGHTNESS_SCALE) / 255;
            b = (b * GAME_BRIGHTNESS_SCALE) / 255;
            int led_num = ledMatrix.LED_Number[y][x];
            led_strip_set_pixel(led_strip, led_num, r, g, b);
        }
    }
    led_strip_refresh(led_strip);

    // Continuous scroll loop - runs until button pressed
    int step = 0;
    while (1) {
        // Only update text rows (optimization: rows 2-6 where text displays)
        for (int x = 0; x < LED_WIDTH; x++){
            int src = step - (LED_WIDTH - x);
            
            // Restore design underneath text area
            for (int y = 2; y < 7; y++) {
                uint8_t val = splash_design_map[y][x];
                if (val == 0) {
                    // transparent - turn off
                    int led_num = ledMatrix.LED_Number[y][x];
                    led_strip_set_pixel(led_strip, led_num, 0, 0, 0);
                } else {
                    // restore design color
                    uint8_t bidx = (val - 1) % NUM_BLOCKS;
                    uint8_t r, g, b;
                    get_block_rgb(bidx, &r, &g, &b);
                    r = (r * GAME_BRIGHTNESS_SCALE) / 255;
                    g = (g * GAME_BRIGHTNESS_SCALE) / 255;
                    b = (b * GAME_BRIGHTNESS_SCALE) / 255;
                    int led_num = ledMatrix.LED_Number[y][x];
                    led_strip_set_pixel(led_strip, led_num, r, g, b);
                }
            }
            
            // Draw text on top if visible
            if (src >= 0 && src < total_cols){
                uint8_t col = cols[src];
                for (int y = 0; y < 5; y++){
                    if (col & (1 << y)){
                        int gy = 2 + y;
                        int led_num = ledMatrix.LED_Number[gy][x];
                        uint8_t brightness = (SPLASH_BRIGHTNESS_SCALE * 255) / 255;
                        led_strip_set_pixel(led_strip, led_num, brightness, brightness, brightness);
                    }
                }
            }
        }

        led_strip_refresh(led_strip);
        
        // Check for button press
        gpio_num_t ev;
        if (controls_get_event(&ev) || 
            check_button_pressed(BTN_LEFT) || 
            check_button_pressed(BTN_RIGHT) || 
            check_button_pressed(BTN_ROTATE) || 
            check_button_pressed(BTN_FASTER)) {
            break;
        }
        
        vTaskDelay(pdMS_TO_TICKS(SPLASH_SCROLL_DELAY_MS));
        step++;
        
        // Loop wraps continuously
        if (step >= total_cols + LED_WIDTH) {
            step = 0;
        }
    }

    free(cols);
}

void splash_clear(void) {
    led_strip_clear(led_strip);
    led_strip_refresh(led_strip);
}


