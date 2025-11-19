#include "Controls.h"
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"  // für xTaskGetTickCount()
#include "driver/gpio.h"
#include <stdint.h>
#include <stdbool.h>
#include "Globals.h"
#include "freertos/queue.h"
#include <stdio.h>

// Queue for button events from ISR
static QueueHandle_t s_button_queue = NULL;

// Prototype for ISR handler (defined after init_controls)
static void IRAM_ATTR gpio_isr_handler(void* arg);

// Use global debounce setting from Globals.h

// Zuordnung der Buttons zu GPIOs
#define BTN_LEFT    GPIO_NUM_4
#define BTN_RIGHT   GPIO_NUM_5
#define BTN_ROTATE  GPIO_NUM_6
#define BTN_FASTER  GPIO_NUM_7
void init_controls(void) {
    gpio_config_t io_conf = {
        .pin_bit_mask = (1ULL << BTN_LEFT) | (1ULL << BTN_RIGHT) |
                        (1ULL << BTN_ROTATE) | (1ULL << BTN_FASTER),
        .mode = GPIO_MODE_INPUT,
        .pull_up_en = GPIO_PULLUP_ENABLE,
        .pull_down_en = GPIO_PULLDOWN_DISABLE,
        .intr_type = GPIO_INTR_NEGEDGE
    };
    gpio_config(&io_conf);

    // Create queue for ISR events
    if (s_button_queue == NULL) {
        s_button_queue = xQueueCreate(8, sizeof(gpio_num_t));
    }

    // Install ISR service and add handlers
    gpio_install_isr_service(0);
    // Use a single ISR handler function that forwards the gpio number to the queue
    // defined below (IRAM_ATTR so it can be called from ISR context).
    gpio_isr_handler_add(BTN_LEFT, gpio_isr_handler, (void*)(intptr_t)BTN_LEFT);
    gpio_isr_handler_add(BTN_RIGHT, gpio_isr_handler, (void*)(intptr_t)BTN_RIGHT);
    gpio_isr_handler_add(BTN_ROTATE, gpio_isr_handler, (void*)(intptr_t)BTN_ROTATE);
    gpio_isr_handler_add(BTN_FASTER, gpio_isr_handler, (void*)(intptr_t)BTN_FASTER);
}

// ISR handler: runs in interrupt context and enqueues the gpio number.
static void IRAM_ATTR gpio_isr_handler(void* arg)
{
    if (s_button_queue == NULL) return;
    gpio_num_t g = (gpio_num_t)(intptr_t)arg;
    BaseType_t xHigherPriorityTaskWoken = pdFALSE;
    xQueueSendFromISR(s_button_queue, &g, &xHigherPriorityTaskWoken);
    if (xHigherPriorityTaskWoken) portYIELD_FROM_ISR();
}

bool check_button_pressed(gpio_num_t gpio) {
    static uint32_t last_pressed_time[GPIO_NUM_MAX] = {0};  // Für alle GPIOs separat
    if (gpio_get_level(gpio) == 0) {  // aktiv LOW
        uint32_t now = xTaskGetTickCount() * portTICK_PERIOD_MS;
        if (now - last_pressed_time[gpio] > BUTTON_DEBOUNCE_MS) {
            last_pressed_time[gpio] = now;
            const char *btn_name = "UNKNOWN";
            if (gpio == BTN_LEFT) btn_name = "LEFT";
            else if (gpio == BTN_RIGHT) btn_name = "RIGHT";
            else if (gpio == BTN_ROTATE) btn_name = "ROTATE";
            else if (gpio == BTN_FASTER) btn_name = "FASTER";
            printf("[Button] %s pressed (GPIO %d)\n", btn_name, gpio);
            return true;
        }
    }
    return false;
}

bool controls_get_event(gpio_num_t *out_gpio) {
    if (s_button_queue == NULL) return false;
    gpio_num_t g;
    if (xQueueReceive(s_button_queue, &g, 0) == pdTRUE) {
        // Debounce in task context
        static uint32_t last_pressed_time[GPIO_NUM_MAX] = {0};
        uint32_t now = xTaskGetTickCount() * portTICK_PERIOD_MS;
        if (now - last_pressed_time[g] > BUTTON_DEBOUNCE_MS) {
            last_pressed_time[g] = now;
            const char *btn_name = "UNKNOWN";
            if (g == BTN_LEFT) btn_name = "LEFT";
            else if (g == BTN_RIGHT) btn_name = "RIGHT";
            else if (g == BTN_ROTATE) btn_name = "ROTATE";
            else if (g == BTN_FASTER) btn_name = "FASTER";
            printf("[Event] %s event from queue (GPIO %d)\n", btn_name, g);
            *out_gpio = g;
            return true;
        }
        return false;
    }
    return false;
}

bool controls_wait_event(gpio_num_t *out_gpio, TickType_t ticks_to_wait) {
    if (s_button_queue == NULL) return false;
    gpio_num_t g;
    if (xQueueReceive(s_button_queue, &g, ticks_to_wait) == pdTRUE) {
        static uint32_t last_pressed_time[GPIO_NUM_MAX] = {0};
        uint32_t now = xTaskGetTickCount() * portTICK_PERIOD_MS;
        if (now - last_pressed_time[g] > BUTTON_DEBOUNCE_MS) {
            last_pressed_time[g] = now;
            *out_gpio = g;
            return true;
        }
    }
    return false;
}

bool controls_all_buttons_pressed(void) {
    // Active low: pressed == 0
    if (gpio_get_level(BTN_LEFT) == 0 && gpio_get_level(BTN_RIGHT) == 0 &&
        gpio_get_level(BTN_ROTATE) == 0 && gpio_get_level(BTN_FASTER) == 0) {
        return true;
    }
    return false;
}

void controls_disable_isr(void) {
    printf("[Controls] Disabling ISR for all buttons\n");
    gpio_isr_handler_remove(BTN_LEFT);
    gpio_isr_handler_remove(BTN_RIGHT);
    gpio_isr_handler_remove(BTN_ROTATE);
    gpio_isr_handler_remove(BTN_FASTER);
    // Disable GPIO interrupts completely
    gpio_intr_disable(BTN_LEFT);
    gpio_intr_disable(BTN_RIGHT);
    gpio_intr_disable(BTN_ROTATE);
    gpio_intr_disable(BTN_FASTER);
}

void controls_enable_isr(void) {
    printf("[Controls] Enabling ISR for all buttons\n");
    // Re-enable GPIO interrupts
    gpio_intr_enable(BTN_LEFT);
    gpio_intr_enable(BTN_RIGHT);
    gpio_intr_enable(BTN_ROTATE);
    gpio_intr_enable(BTN_FASTER);
    gpio_isr_handler_add(BTN_LEFT, gpio_isr_handler, (void*)(intptr_t)BTN_LEFT);
    gpio_isr_handler_add(BTN_RIGHT, gpio_isr_handler, (void*)(intptr_t)BTN_RIGHT);
    gpio_isr_handler_add(BTN_ROTATE, gpio_isr_handler, (void*)(intptr_t)BTN_ROTATE);
    gpio_isr_handler_add(BTN_FASTER, gpio_isr_handler, (void*)(intptr_t)BTN_FASTER);
}
