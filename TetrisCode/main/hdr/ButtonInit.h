#ifndef BUTTON_INIT_H
#define BUTTON_INIT_H

#include "driver/gpio.h"

// Initialisiert einen GPIO als Button-Eingang (nach GND schaltend)
// Aktiviert internen Pull-up, deaktiviert Pull-down und Interrupts
void init_button(gpio_num_t gpio);

#endif // BUTTON_INIT_H
