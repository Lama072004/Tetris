//////////////////////////////////////////////////////////////////////////////////////////////////
/*
Info:
MatrixNummer.c und MatrixNummer.h enthalten die Nummern welche die LEDs in der Matrix 
haben basierend auf der Verkabelung der 6 8*8 Matrizen

LedMatrixInit.c und LedMatrixInit.h initialisieren die MatrixNummer Struktur mit den LED Nummern

In dem Ordner src befinden sich alle .c Dateien und in dem Ordner hdr alle .h Dateien
*/
//////////////////////////////////////////////////////////////////////////////////////////////////

#include <stdio.h>
#include <stdint.h>
#include <stdbool.h>
#include <stdlib.h>
#include <time.h>
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "driver/gpio.h"
#include "esp_log.h"
#include "led_strip.h"
#include "sdkconfig.h"
#include "esp_random.h"

#include "Globals.h"
#include "LedMatrixInit.h"
#include "MatrixNummer.h"
#include "Controls.h"
#include "GameLoop.h"
#include "Grid.h"
#include "Score.h"
#include "DisplayInit.h"
#include "Splash.h"
#include "ThemeSong.h"

//////////////////////////////////////////////////////////////////////////////////////////////////

#define LED_GPIO   1
#define LED_WIDTH 16
#define LED_HEIGHT 24

//////////////////////////////////////////////////////////////////////////////////////////////////

// Globale Matrix, damit alle Funktionen darauf zugreifen können
MATRIX ledMatrix;
led_strip_handle_t led_strip;

//////////////////////////////////////////////////////////////////////////////////////////////////

void setup_led_strip() {
    led_strip_config_t strip_config = {
        .strip_gpio_num = LED_GPIO,
        .max_leds = LED_HEIGHT * LED_WIDTH,
    };

    led_strip_rmt_config_t rmt_config = {
        .resolution_hz = 10 * 1000 * 1000,
        .flags.with_dma = false,
    };

    ESP_ERROR_CHECK(led_strip_new_rmt_device(&strip_config, &rmt_config, &led_strip));
    led_strip_clear(led_strip);
    // Ensure physical LEDs are updated after initialization
    led_strip_refresh(led_strip);
}

//////////////////////////////////////////////////////////////////////////////////////////////////

void app_main(void){
    // LED Matrix initialisieren
    setup_led_strip();
    LedMatrixInit(LED_HEIGHT, LED_WIDTH, ledMatrix.LED_Number);

    // Seed random number generator for random block spawning
    // Use a combination of esp_random() and a static counter to ensure variation across reboots
    static uint32_t seed_counter = 12345;
    srand(esp_random() ^ seed_counter);
    seed_counter += 1111;  // Increment for next potential seed

    // Show splash animation (scrolling text + static doubled blocks)
    splash_show(SPLASH_DURATION_MS);

    // Clear LED matrix after splash
    led_strip_clear(led_strip);
    led_strip_refresh(led_strip);

    // Grid und Score initialisieren
    grid_init();
    score_init();
    score_load_highscore();  // Load highscore from NVS

    // Display initialisieren (can fail gracefully if not connected)
    display_init();
    // Show the loaded highscore on the initial screen (if display is available)
    if (g_disp != NULL) {
        display_reset_and_show_hud(score_get_highscore());
    }

    // Controls initialisieren
    init_controls();

    // -------------------------------
    // Tetris Theme starten (parallel)
    // -------------------------------
    StartTheme();
    // GameLoop starten (Button-Druck kommentiert aus - GameLoop startet direkt)
    start_game_loop();

    // Keep app running (GameLoop task handles the game)
    while(1){
        vTaskDelay(pdMS_TO_TICKS(1000));
    }
}
