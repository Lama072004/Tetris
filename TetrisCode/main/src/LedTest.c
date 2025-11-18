/*

#include <stdio.h>
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "driver/gpio.h"
#include "esp_log.h"
#include "led_strip.h"
#include "sdkconfig.h"

#define LED_GPIO   1
#define NUM_LEDS   384

static const char *TAG = "LED_APP";
static led_strip_handle_t led_strip;  // globale LED-Instanz

void led_einzeln(int number) {
    led_strip_clear(led_strip);
    int x = rand() % 255;
    int y = rand() % 255;
    int z = rand() % 255;
    int index = number % NUM_LEDS;
    led_strip_set_pixel(led_strip, index, x, y, z);
    led_strip_refresh(led_strip);

    vTaskDelay(pdMS_TO_TICKS(100));
}

void setup_led_strip() {
    led_strip_config_t strip_config = {
        .strip_gpio_num = LED_GPIO,
        .max_leds = NUM_LEDS,
    };

    led_strip_rmt_config_t rmt_config = {
        .resolution_hz = 10 * 1000 * 1000, // 10 MHz
        .flags.with_dma = false,
    };

    ESP_ERROR_CHECK(led_strip_new_rmt_device(&strip_config, &rmt_config, &led_strip));
    led_strip_clear(led_strip);
}

void app_main(void) {
    setup_led_strip();

    while (1) {
        for (int LED_Number = 0; LED_Number < NUM_LEDS; LED_Number++) {
            led_einzeln(LED_Number);
            ESP_LOGI(TAG, "LED Number = %d", LED_Number);
        }
    }
}


*/