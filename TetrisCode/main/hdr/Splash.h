#ifndef SPLASH_H
#define SPLASH_H

#include <stdint.h>

// Show the splash animation on the LED matrix. duration_ms controls how long the
// splash remains visible (use SPLASH_DURATION_MS constant from Globals.h).
void splash_show(uint32_t duration_ms);

// Clear the splash image from the LEDs (used when the game starts)
void splash_clear(void);

#endif // SPLASH_H
