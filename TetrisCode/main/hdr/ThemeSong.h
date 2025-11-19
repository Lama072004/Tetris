#ifndef THEMESONG_H
#define THEMESONG_H

#include <stdint.h>

/* --------------------------------------------------------------------------
   Struktur für eine einzelne Note oder Pause
   freq_hz = Frequenz der Note in Hz (0 für Pause)
   duration_ms = Dauer der Note/Pause in Millisekunden
   ------------------------------------------------------------------------ */
typedef struct {
    uint32_t freq_hz;
    uint32_t duration_ms;
} buzzer_musical_score_t;

/* --------------------------------------------------------------------------
   Startet die Tetris-Theme-Melodie wiederholend auf dem Buzzer
   Implementierung muss RMT oder einen vergleichbaren Treiber verwenden
   ------------------------------------------------------------------------ */
void StartTheme(void);

#endif // THEMESONG_H
