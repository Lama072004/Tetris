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
   Musik läuft standardmäßig, kann mit theme_pause()/theme_resume() kontrolliert werden
   ------------------------------------------------------------------------ */
void StartTheme(void);

/* --------------------------------------------------------------------------
   Musik pausieren (z.B. bei Game Over oder Menü)
   Verwendet Event-Group für sichere Synchronisation zwischen Tasks
   
   VERBESSERUNG: Erlaubt, Musik zu pausieren/fortsetzen ohne Task zu löschen
   Dies spart Ressourcen und verhindert Audio-Glitches beim Neustart
   -------------------------------------------------------------------------- */
void theme_pause(void);

/* --------------------------------------------------------------------------
   Musik fortsetzen (Resume nach Pause)
   -------------------------------------------------------------------------- */
void theme_resume(void);

/* --------------------------------------------------------------------------
   MULTI-SONG SYSTEM
   
   Verschiedene verfügbare Melodien können gewechselt werden
   -------------------------------------------------------------------------- */

enum ThemeSongType {
    THEME_TETRIS = 0,      // Original Tetris Melodie
    THEME_STARWARS = 1,    // Star Wars Theme
    THEME_MARIO = 2,       // Super Mario Bros
    THEME_SILENCE = 3,     // Keine Musik (Pause)
    THEME_LAST = 4         // Sentinel (um zurück zu TETRIS zu springen)
};

/* Wechsele zur nächsten Musik (mit Cycling: TETRIS → STARWARS → MARIO → SILENCE → TETRIS)
   Diese Funktion wird durch die Tastenkombination (alle 4 Buttons) aufgerufen */
void theme_next_song(void);

/* Gebe aktuelle Song-Nummer zurück */
int theme_get_current_song(void);

/* Direkter Song-Wechsel zu bestimmtem Index */
void theme_set_song(int song_index);

#endif // THEMESONG_H
