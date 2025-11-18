#ifndef CONTROLS_H
#define CONTROLS_H

#include <stdbool.h>
#include "driver/gpio.h"
// TickType_t is defined in FreeRTOS headers
#include "freertos/FreeRTOS.h"

#define BTN_LEFT    GPIO_NUM_4
#define BTN_RIGHT   GPIO_NUM_5
#define BTN_ROTATE  GPIO_NUM_6
#define BTN_FASTER  GPIO_NUM_7

void init_controls(void);
// Non-blocking: try to get a button event from ISR queue. Returns true if event available.
// out_gpio will be set to the gpio number of the pressed button.
bool controls_get_event(gpio_num_t *out_gpio);

// Blocking wait for event with timeout in ticks
bool controls_wait_event(gpio_num_t *out_gpio, TickType_t ticks_to_wait);

// Deprecated: polling helper (kept for compatibility)
bool check_button_pressed(gpio_num_t gpio);

// Returns true if all defined buttons are currently pressed (active low)
bool controls_all_buttons_pressed(void);

#endif // CONTROLS_H
