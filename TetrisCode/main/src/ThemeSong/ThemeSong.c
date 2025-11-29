#include "driver/rmt_tx.h"
#include "musical_score_encoder.h"
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "esp_log.h"
#include "Globals.h"

#define SPEED THEME_SONG_SPEED

static const char *TAG = "ThemeSong";

/* --------------------------------------------------------------------------
   NOTE DEFINITIONS (frequencies in Hz)
   ------------------------------------------------------------------------ */
#define Pause 1
#define C2  65
#define CS2 69
#define D2  73
#define DS2 78
#define E2  82
#define F2  87
#define FS2 93
#define G2  98
#define GS2 104
#define A2  110
#define AS2 117
#define B2  123

#define C3  131
#define CS3 139
#define D3  147
#define DS3 156
#define E3  165
#define F3  175
#define FS3 185
#define G3  196
#define GS3 208
#define A3  220
#define AS3 233
#define B3  247

#define C4  262
#define CS4 277
#define D4  294
#define DS4 311
#define E4  330
#define F4  349
#define FS4 370
#define G4  392
#define GS4 415
#define A4  440
#define AS4 466
#define B4  494

#define C5  523
#define CS5 554
#define D5  587
#define DS5 622
#define E5  659
#define F5  698
#define FS5 740
#define G5  784
#define GS5 831
#define A5  880
#define AS5 932
#define B5  988

/* --------------------------------------------------------------------------
   Score
   ------------------------------------------------------------------------ */
static const buzzer_musical_score_t score[] = {
    {E4, 500*SPEED}, {B3, 250*SPEED}, {C4, 250*SPEED}, {D4, 500*SPEED},
    {C4, 250*SPEED}, {B3, 250*SPEED}, {A3, 500*SPEED}, {A3, 250*SPEED},
    {C4, 250*SPEED}, {E4, 500*SPEED}, {D4, 250*SPEED}, {C4, 250*SPEED},
    {B3, 500*SPEED}, {B3, 250*SPEED}, {C4, 250*SPEED}, {D4, 500*SPEED},
    {E4, 500*SPEED}, {C4, 500*SPEED}, {A3, 500*SPEED}, {A3, 500*SPEED},
    {Pause, 1000*SPEED},
    {D4, 750*SPEED}, {F4, 250*SPEED}, {A4, 500*SPEED}, {G4, 250*SPEED},
    {F4, 250*SPEED}, {E4, 750*SPEED}, {C4, 250*SPEED}, {E4, 500*SPEED},
    {D4, 250*SPEED}, {C4, 250*SPEED}, {B3, 500*SPEED}, {B3, 250*SPEED},
    {C4, 250*SPEED}, {D4, 500*SPEED}, {E4, 500*SPEED}, {C4, 500*SPEED},
    {A3, 500*SPEED}, {A3, 500*SPEED},
    {Pause, 500*SPEED},{Pause, 1000*SPEED},

    {E5, 500*SPEED}, {B4, 250*SPEED}, {C5, 250*SPEED}, {D5, 500*SPEED},
    {C5, 250*SPEED}, {B4, 250*SPEED}, {A4, 500*SPEED}, {A4, 250*SPEED},
    {C5, 250*SPEED}, {E5, 500*SPEED}, {D5, 250*SPEED}, {C5, 250*SPEED},
    {B4, 500*SPEED}, {B4, 250*SPEED}, {C5, 250*SPEED}, {D5, 500*SPEED},
    {E5, 500*SPEED}, {C5, 500*SPEED}, {A4, 500*SPEED}, {A4, 500*SPEED},
    {Pause, 1000*SPEED},
    {D5, 750*SPEED}, {F5, 250*SPEED}, {A5, 500*SPEED}, {G5, 250*SPEED},
    {F5, 250*SPEED}, {E5, 750*SPEED}, {C5, 250*SPEED}, {E5, 500*SPEED},
    {D5, 250*SPEED}, {C5, 250*SPEED}, {B4, 500*SPEED}, {B4, 250*SPEED},
    {C5, 250*SPEED}, {D5, 500*SPEED}, {E5, 500*SPEED}, {C5, 500*SPEED},
    {A4, 500*SPEED}, {A4, 500*SPEED},
    {Pause, 500*SPEED},{Pause, 1000*SPEED},
};

/* --------------------------------------------------------------------------
   Task-Funktion für wiederholtes Abspielen
   ------------------------------------------------------------------------ */
static void ThemeTask(void *arg) {
    ESP_LOGI(TAG, "Theme task started");

    // RMT-Setup
    rmt_channel_handle_t buzzer_chan = NULL;
    rmt_tx_channel_config_t tx_chan_config = {
        .clk_src = RMT_CLK_SRC_DEFAULT,
        .gpio_num = BUZZER_GPIO_PIN,
        .mem_block_symbols = 64,
        .resolution_hz = RMT_BUZZER_RESOLUTION_HZ,
        .trans_queue_depth = 10,
    };
    ESP_ERROR_CHECK(rmt_new_tx_channel(&tx_chan_config, &buzzer_chan));

    rmt_encoder_handle_t score_encoder = NULL;
    musical_score_encoder_config_t encoder_config = {
        .resolution = RMT_BUZZER_RESOLUTION_HZ
    };
    ESP_ERROR_CHECK(rmt_new_musical_score_encoder(&encoder_config, &score_encoder));
    ESP_ERROR_CHECK(rmt_enable(buzzer_chan));

    const size_t score_entries = sizeof(score) / sizeof(score[0]);

    while(1) {
        for (size_t i = 0; i < score_entries; ++i) {
            uint32_t freq = score[i].freq_hz;
            uint32_t dur_ms = score[i].duration_ms;

            if (freq == 0) {
                vTaskDelay(pdMS_TO_TICKS(dur_ms));
                continue;
            }

            uint64_t prod = (uint64_t)dur_ms * (uint64_t)freq;
            uint32_t loop_count = (uint32_t)(prod / 1000ULL);
            if (loop_count == 0) loop_count = 1;

            rmt_transmit_config_t tx_config = { .loop_count = loop_count };
            ESP_ERROR_CHECK(rmt_transmit(buzzer_chan, score_encoder, &score[i],
                                        sizeof(buzzer_musical_score_t), &tx_config));
        }
    }
}

/* --------------------------------------------------------------------------
   StartTheme
   ------------------------------------------------------------------------ */
void StartTheme(void) {
    // Task nur einmal erstellen
    static bool task_started = false;
    if (!task_started) {
        xTaskCreate(ThemeTask, "ThemeTask", 4096, NULL, 5, NULL);
        task_started = true;
    }
}
