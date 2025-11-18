#ifndef GLOBALS_H
#define GLOBALS_H

#include <stdint.h>
#include <stdbool.h>
#include "led_strip.h"

//////////////////////////////////////////////////////////////////////////////////////////////////
// MATRIX CONFIGURATION
//////////////////////////////////////////////////////////////////////////////////////////////////
#define LED_WIDTH 16
#define LED_HEIGHT 24

//////////////////////////////////////////////////////////////////////////////////////////////////
// GAME TIMING CONFIGURATION (all in milliseconds)
//////////////////////////////////////////////////////////////////////////////////////////////////
// Block fall speed: how often a block falls one row (lower = faster)
#define FALL_INTERVAL_MS 300

// Render/refresh frequency: how often LEDs update (lower = smoother, ~60 FPS = 16ms)
#define RENDER_INTERVAL_MS 16

// Main loop frequency: how often input is checked (lower = more responsive, should be <= RENDER_INTERVAL_MS)
#define LOOP_INTERVAL_MS 5

// Splash animation duration: how long the TETRIS startup screen shows
#define SPLASH_DURATION_MS 4000

// Splash scroll delay between frames
#define SPLASH_SCROLL_DELAY_MS 40

//////////////////////////////////////////////////////////////////////////////////////////////////
// DISPLAY BRIGHTNESS CONFIGURATION
//////////////////////////////////////////////////////////////////////////////////////////////////
// Brightness scale for game blocks (0-255, 128 = 50%, 255 = 100%)
#define GAME_BRIGHTNESS_SCALE 125

// Brightness scale for splash text (0-255)
#define SPLASH_BRIGHTNESS_SCALE 10

// Game Over blink brightness (0-255, 50% = 0x80)
#define GAME_OVER_BLINK_R 128
#define GAME_OVER_BLINK_G 0
#define GAME_OVER_BLINK_B 0

// Game Over blink duration (on time in ms)
#define GAME_OVER_BLINK_ON_MS 300

// Game Over blink duration (off time in ms)
#define GAME_OVER_BLINK_OFF_MS 300

// Number of times to blink on Game Over
#define GAME_OVER_BLINK_COUNT 3

//////////////////////////////////////////////////////////////////////////////////////////////////
// INPUT DEBOUNCING
//////////////////////////////////////////////////////////////////////////////////////////////////
// Button debounce time in milliseconds (faster block movement during game)
// Reduced so gameplay feels more responsive and start requires shorter press
#define BUTTON_DEBOUNCE_MS 50

//////////////////////////////////////////////////////////////////////////////////////////////////
// GRID & COLLISION CONFIGURATION
//////////////////////////////////////////////////////////////////////////////////////////////////
#define GRID_WIDTH 16
#define GRID_HEIGHT 24

#define NUM_BLOCKS 7
// Brightness divider used to scale down 0-255 color values
#define BRIGHTNESSDIV 10

// Block type indices (use these to refer to colors / block types)
enum BlockType {
    BLOCK_I = 0,
    BLOCK_J = 1,
    BLOCK_L = 2,
    BLOCK_O = 3,
    BLOCK_S = 4,
    BLOCK_T = 5,
    BLOCK_Z = 6
};

// Centralized color table (r,g,b) per block type. Defined in Globals.c
extern const uint8_t block_colors[NUM_BLOCKS][3];

typedef struct {
    uint16_t LED_Number[LED_HEIGHT][LED_WIDTH];
    uint8_t red;
    uint8_t green;
    uint8_t blue;
    bool isFilled;
} MATRIX;

extern MATRIX ledMatrix;
extern led_strip_handle_t led_strip;

#endif // GLOBALS_H

