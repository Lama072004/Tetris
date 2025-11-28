/**
 * @file GameLoop.c
 * @brief Tetris Haupt-Spielschleife mit State Machine
 * 
 * Dieses Modul implementiert die zentrale Spiellogik als FreeRTOS Task:
 * - State Machine: WAIT → RUNNING → GAME_OVER → WAIT
 * - Input-Verarbeitung (Buttons)
 * - Block-Physics (Bewegung, Rotation, Fall)
 * - Rendering (60 FPS, optimiert)
 * - Emergency Reset (4-Button-Kombination)
 */

#include "Globals.h"
#include "Blocks.h"
#include "Grid.h"
#include "Controls.h"
#include "Score.h"
#include "SpeedManager.h"
#include "DisplayInit.h"
#include "Splash.h"
#include "led_strip.h"
#include <stdlib.h>
#include <stdio.h>
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "esp_random.h"

// ============================================================================
// EXTERNE VARIABLEN
// ============================================================================

extern MATRIX ledMatrix;           // LED-Matrix Mapping (aus main.c)
extern led_strip_handle_t led_strip;  // WS2812B Strip Handle

// ============================================================================
// PRIVATE VARIABLEN
// ============================================================================

/** @brief Aktuell fallender Tetromino-Block */
static TetrisBlock current_block;

/** @brief Flag: Game Over erkannt (wird von handle_game_over() gesetzt) */
static volatile int game_over_flag = 0;

/** @brief Anzahl dynamischer Pixel vom letzten Frame (für optimiertes Rendering) */
static int prev_dynamic_count = 0;

/** @brief Positionen der dynamischen Pixel [y,x] für Restore im nächsten Frame */
static int prev_dynamic_pos[GRID_WIDTH * GRID_HEIGHT][2];

// ============================================================================
// FORWARD DECLARATIONS
// ============================================================================

static void handle_game_over(void);
static void render_grid(void);
static void spawn_block(void);
static void reset_game_state(void);
static void wait_for_restart(void);

// ============================================================================
// RENDERING
// ============================================================================

/**
 * @brief Rendert das Spielfeld auf die LED-Matrix (optimiert)
 * 
 * Optimierungstechnik:
 * 1. Vorherige dynamische Pixel (aktueller Block) werden restauriert
 * 2. Nur statische Farben aus grid[][] werden geschrieben
 * 3. Neuer Block wird an aktueller Position gezeichnet
 * 4. Nur geänderte Pixel werden aktualisiert (kein led_strip_clear!)
 * 
 * Resultat: Flimmerfreies Rendering bei 60 FPS
 */
static void render_grid(void) {
    // Schritt 1: Restauriere vorherige dynamische Pixel zurück auf statische Farben
    for (int i = 0; i < prev_dynamic_count; i++) {
        int ry = prev_dynamic_pos[i][0];
        int rx = prev_dynamic_pos[i][1];
        int led_num = ledMatrix.LED_Number[ry][rx];
        
        if (grid[ry][rx] > 0) {
            // Statischer Block an dieser Position → Farbe setzen
            uint8_t r, g, b;
            get_block_rgb(grid[ry][rx] - 1, &r, &g, &b);
            r = (r * GAME_BRIGHTNESS_SCALE) / 255;
            g = (g * GAME_BRIGHTNESS_SCALE) / 255;
            b = (b * GAME_BRIGHTNESS_SCALE) / 255;
            led_strip_set_pixel(led_strip, led_num, r, g, b);
        } else {
            // Leer → Pixel ausschalten
            led_strip_set_pixel(led_strip, led_num, 0, 0, 0);
        }
    }
    prev_dynamic_count = 0;

    // Schritt 2: Zeichne aktuellen Block (dynamisch)
    for (int by = 0; by < 4; by++) {
        for (int bx = 0; bx < 4; bx++) {
            if (current_block.shape[by][bx]) {
                int gx = current_block.x + bx;
                int gy = current_block.y + by;
                
                // Bounds-Check (Block kann teilweise außerhalb sein)
                if (gx >= 0 && gx < GRID_WIDTH && gy >= 0 && gy < GRID_HEIGHT) {
                    // Block-Farbe mit Helligkeit skalieren
                    uint8_t r, g, b;
                    get_block_rgb(current_block.color, &r, &g, &b);
                    r = (r * GAME_BRIGHTNESS_SCALE) / 255;
                    g = (g * GAME_BRIGHTNESS_SCALE) / 255;
                    b = (b * GAME_BRIGHTNESS_SCALE) / 255;
                    
                    int led_num = ledMatrix.LED_Number[gy][gx];
                    led_strip_set_pixel(led_strip, led_num, r, g, b);
                    
                    // Position merken für nächsten Frame
                    if (prev_dynamic_count < (GRID_WIDTH * GRID_HEIGHT)) {
                        prev_dynamic_pos[prev_dynamic_count][0] = gy;
                        prev_dynamic_pos[prev_dynamic_count][1] = gx;
                        prev_dynamic_count++;
                    }
                }
            }
        }
    }

    // Schritt 3: LED-Matrix aktualisieren (RMT sendet Daten an WS2812B)
    led_strip_refresh(led_strip);
}

// ============================================================================
// BLOCK SPAWNING & GAME OVER
// ============================================================================

/**
 * @brief Spawnt einen neuen zufälligen Tetromino-Block
 * 
 * Algorithmus:
 * 1. Zufälligen Block-Typ wählen (0-6)
 * 2. Spawn-Position finden (bevorzugt: Mitte-oben)
 * 3. Kollisionsprüfung beim Spawn
 * 4. Falls kein Platz gefunden → Game Over
 * 
 * Der Algorithmus versucht mehrere x-Positionen (Mitte, links, rechts)
 * und bei Bedarf auch y = -1 (teilweise oberhalb sichtbar)
 */
static void spawn_block(void) {
    // Zufälligen Block-Typ wählen (0-6: I, J, L, O, S, T, Z)
    int block_type = esp_random() % 7;
    TetrisBlock candidate = blocks[block_type][0];

    // Versuche Spawn-Position zu finden (bevorzugt Mitte)
    int preferred = GRID_WIDTH / 2 - 2;
    int found = 0;
    int best_x = preferred;

    // Suche in Offsets: 0, -1, +1, -2, +2, ...
    for (int offset = 0; offset <= GRID_WIDTH && !found; offset++) {
        if (offset == 0) {
            // Zuerst Mitte versuchen
            candidate.x = preferred;
            candidate.y = 0;
            if (!grid_check_collision(&candidate)) {
                best_x = preferred;
                found = 1;
                break;
            }
        } else {
            // Links und rechts von Mitte versuchen
            int positions[2] = {preferred - offset, preferred + offset};
            for (int i = 0; i < 2; i++) {
                int tx = positions[i];
                if (tx < 0 || tx > GRID_WIDTH - 4) continue;
                
                candidate.x = tx;
                candidate.y = 0;
                if (!grid_check_collision(&candidate)) {
                    best_x = tx;
                    found = 1;
                    break;
                }
            }
        }
    }

    // Falls y=0 nicht funktioniert, versuche y=-1 (teilweise oberhalb)
    if (!found) {
        for (int offset = 0; offset <= GRID_WIDTH && !found; offset++) {
            if (offset == 0) {
                candidate.x = preferred;
                candidate.y = -1;
                if (!grid_check_collision(&candidate)) {
                    best_x = preferred;
                    found = 1;
                    break;
                }
            } else {
                int positions[2] = {preferred - offset, preferred + offset};
                for (int i = 0; i < 2; i++) {
                    int tx = positions[i];
                    if (tx < 0 || tx > GRID_WIDTH - 4) continue;
                    
                    candidate.x = tx;
                    candidate.y = -1;
                    if (!grid_check_collision(&candidate)) {
                        best_x = tx;
                        found = 1;
                        break;
                    }
                }
            }
        }
    }

    // Kein Platz gefunden → Game Over
    if (!found) {
        handle_game_over();
        return;
    }

    // Block erfolgreich spawned
    current_block = blocks[block_type][0];
    current_block.x = best_x;
    current_block.y = (candidate.y < 0) ? -1 : 0;
    assign_block_color(&current_block, block_type);
}

/**
 * @brief Game Over Handler
 * 
 * Wird aufgerufen wenn kein Platz für neuen Block:
 * 1. Highscore aktualisieren und in NVS speichern
 * 2. Game Over auf Display anzeigen
 * 3. Blink-Animation (3× rot, je 300ms on/off)
 * 4. game_over_flag setzen → Hauptschleife startet Neustart-Sequenz
 */
static void handle_game_over(void) {
    // Highscore aktualisieren (falls neuer Rekord)
    score_update_highscore();
    
    // Game Over Screen auf OLED anzeigen
    display_show_game_over(score_get(), score_get_highscore());
    
    // Blink-Animation: LED-Matrix rot blinken lassen
    for (int blink = 0; blink < GAME_OVER_BLINK_COUNT; blink++) {
        // An: Alle LEDs rot
        for (int y = 0; y < GRID_HEIGHT; y++) {
            for (int x = 0; x < GRID_WIDTH; x++) {
                int led = ledMatrix.LED_Number[y][x];
                led_strip_set_pixel(led_strip, led, 
                    GAME_OVER_BLINK_R, GAME_OVER_BLINK_G, GAME_OVER_BLINK_B);
            }
        }
        led_strip_refresh(led_strip);
        vTaskDelay(pdMS_TO_TICKS(GAME_OVER_BLINK_ON_MS));

        // Aus: Alle LEDs schwarz
        led_strip_clear(led_strip);
        led_strip_refresh(led_strip);
        vTaskDelay(pdMS_TO_TICKS(GAME_OVER_BLINK_OFF_MS));
    }

    // Flag setzen → Hauptschleife reagiert darauf
    game_over_flag = 1;
}

// ============================================================================
// GAME STATE MANAGEMENT
// ============================================================================

/**
 * @brief Setzt den Spielzustand zurück (für Neustart oder Reset)
 * 
 * Wird aufgerufen bei:
 * - Emergency Reset (4 Buttons gedrückt)
 * - Game Over → Neustart
 * - Normaler Spielstart nach Splash
 */
static void reset_game_state(void) {
    grid_init();
    score_init();
    speed_manager_reset();
    display_reset_and_show_hud(score_get_highscore());
}

/**
 * @brief Wartet auf Neustart nach Reset oder Game Over
 * 
 * Sequenz:
 * 1. Queue drainieren (alte Events entfernen)
 * 2. Splash 2 Sekunden anzeigen (Inputs ignoriert)
 * 3. Kontinuierliches Queue-Drain während 500ms
 * 4. Warten bis alle Buttons released
 * 5. Interactive Splash (scrollt bis Button gedrückt)
 * 6. Warten auf Button-Release
 * 7. Spiel starten
 */
static void wait_for_restart(void) {
    gpio_num_t ev;
    
    // 1. Queue drainieren
    while (controls_get_event(&ev)) {}
    
    // 2. Splash 2s anzeigen (Inputs ignoriert)
    splash_show_duration(2000);
    
    // 3. Queue drainieren + 500ms kontinuierlich
    printf("[GameLoop] Draining queue after 2s splash...\n");
    int drained = 0;
    while (controls_get_event(&ev)) {
        drained++;
    }
    printf("[GameLoop] Drained %d events from queue\n", drained);
    
    printf("[GameLoop] Waiting 500ms while continuously draining queue...\n");
    uint32_t drain_start = xTaskGetTickCount() * portTICK_PERIOD_MS;
    while ((xTaskGetTickCount() * portTICK_PERIOD_MS - drain_start) < 500) {
        while (controls_get_event(&ev)) {
            printf("[GameLoop] Caught straggler event during drain: GPIO %d\n", ev);
        }
        vTaskDelay(pdMS_TO_TICKS(10));
    }
    
    // 4. Warten bis alle Buttons released
    vTaskDelay(pdMS_TO_TICKS(50));
    while (check_button_pressed(BTN_LEFT) || check_button_pressed(BTN_RIGHT) ||
           check_button_pressed(BTN_ROTATE) || check_button_pressed(BTN_FASTER)) {
        vTaskDelay(pdMS_TO_TICKS(20));
    }
    
    // 5. Interactive Splash (wartet auf Button)
    splash_show_waiting();
    vTaskDelay(pdMS_TO_TICKS(50));
    
    // 6. Queue drainieren + Warten auf Release
    while (controls_get_event(&ev)) {}
    vTaskDelay(pdMS_TO_TICKS(50));
    while (check_button_pressed(BTN_LEFT) || check_button_pressed(BTN_RIGHT) ||
           check_button_pressed(BTN_ROTATE) || check_button_pressed(BTN_FASTER)) {
        vTaskDelay(pdMS_TO_TICKS(20));
    }
}

// ============================================================================
// MAIN GAME LOOP TASK
// ============================================================================

/**
 * @brief Haupt-Spielschleife als FreeRTOS Task
 * 
 * Task-Parameter:
 * - Priorität: 5 (hoch)
 * - Stack: 4096 Bytes
 * - Polling-Intervall: 5ms (responsive Input)
 * - Render-Intervall: 16ms (60 FPS)
 * - Fall-Intervall: dynamisch (400ms initial, bis 50ms bei Level 10)
 * 
 * State Machine:
 * - WAIT: Warte auf Button zum Starten (Splash scrollt)
 * - RUNNING: Spiel läuft (Input, Physics, Rendering)
 * - GAME_OVER: Übergang zu WAIT nach Animation
 * - EMERGENCY_RESET: Hard-Reset via 4-Button-Combo
 * 
 * @param pvParameters Unused (NULL)
 */
void game_loop_task(void *pvParameters) {
    // ========================================================================
    // INITIALISIERUNG
    // ========================================================================
    
    speed_manager_init();
    reset_game_state();
    
    bool game_running = false;
    uint32_t last_fall_time = 0;
    uint32_t last_render_time = 0;
    
    // ========================================================================
    // HAUPTSCHLEIFE
    // ========================================================================
    
    while (1) {
        uint32_t current_time = xTaskGetTickCount() * portTICK_PERIOD_MS;
        
        // ====================================================================
        // EMERGENCY RESET CHECK (4 Buttons gleichzeitig)
        // ====================================================================
        
        if (controls_all_buttons_pressed()) {
            // 1 Sekunde halten erforderlich (versehentliche Trigger vermeiden)
            vTaskDelay(pdMS_TO_TICKS(1000));
            
            if (controls_all_buttons_pressed()) {
                printf("[GameLoop] Emergency reset triggered (all buttons)\n");
                
                // Hard Reset durchführen
                reset_game_state();
                led_strip_clear(led_strip);
                led_strip_refresh(led_strip);
                game_running = false;
                
                // Neustart-Sequenz
                wait_for_restart();
                
                // Spiel starten
                splash_clear();
                reset_game_state();
                spawn_block();
                game_running = true;
                last_fall_time = xTaskGetTickCount() * portTICK_PERIOD_MS;
                last_render_time = last_fall_time;
                continue;
            }
        }
        
        // ====================================================================
        // GAME OVER CHECK
        // ====================================================================
        
        if (game_over_flag) {
            game_over_flag = 0;
            game_running = false;
            display_reset_and_show_hud(score_get_highscore());
            
            // Neustart-Sequenz
            wait_for_restart();
            
            // Spiel starten
            splash_clear();
            reset_game_state();
            spawn_block();
            game_running = true;
            last_fall_time = xTaskGetTickCount() * portTICK_PERIOD_MS;
            last_render_time = last_fall_time;
            continue;
        }
        
        // ====================================================================
        // WAIT STATE (Spiel noch nicht gestartet)
        // ====================================================================
        
        if (!game_running) {
            // Splash-Animation (scrollt endlos bis Button gedrückt)
            splash_show(SPLASH_DURATION_MS);
            
            // Queue drainieren
            gpio_num_t ev;
            while (controls_get_event(&ev)) {}
            
            // Warten auf Button-Release
            vTaskDelay(pdMS_TO_TICKS(50));
            while (check_button_pressed(BTN_LEFT) || check_button_pressed(BTN_RIGHT) ||
                   check_button_pressed(BTN_ROTATE) || check_button_pressed(BTN_FASTER)) {
                vTaskDelay(pdMS_TO_TICKS(20));
            }
            
            // Warte auf frischen Button-Press
            controls_wait_event(&ev, portMAX_DELAY);
            vTaskDelay(pdMS_TO_TICKS(50));
            
            // Spiel starten
            splash_clear();
            reset_game_state();
            spawn_block();
            game_running = true;
            last_fall_time = xTaskGetTickCount() * portTICK_PERIOD_MS;
            last_render_time = last_fall_time;
            continue;
        }
        
        // ====================================================================
        // RUNNING STATE - INPUT HANDLING
        // ====================================================================
        
        TetrisBlock tmp;
        
        // Links-Bewegung
        if (check_button_pressed(BTN_LEFT)) {
            tmp = current_block;
            tmp.x--;
            if (!grid_check_collision(&tmp)) {
                current_block = tmp;
            }
        }
        
        // Rechts-Bewegung
        if (check_button_pressed(BTN_RIGHT)) {
            tmp = current_block;
            tmp.x++;
            if (!grid_check_collision(&tmp)) {
                current_block = tmp;
            }
        }
        
        // Rotation (O-Block rotiert nicht)
        if (check_button_pressed(BTN_ROTATE) && current_block.color != 3) {
            tmp = current_block;
            rotate_block_90(&tmp);
            if (!grid_check_collision(&tmp)) {
                current_block = tmp;
            }
        }
        
        // Manuelles Fallenlassen (Schneller-Taste)
        if (check_button_pressed(BTN_FASTER)) {
            tmp = current_block;
            tmp.y++;
            if (!grid_check_collision(&tmp)) {
                current_block = tmp;
            }
        }
        
        // ====================================================================
        // AUTOMATIC FALL (Zeit-basiert)
        // ====================================================================
        
        uint32_t fall_interval = speed_manager_get_fall_interval();
        if (current_time - last_fall_time >= fall_interval) {
            last_fall_time = current_time;
            
            tmp = current_block;
            tmp.y++;
            
            if (!grid_check_collision(&tmp)) {
                // Block kann weiter fallen
                current_block = tmp;
            } else {
                // Kollision → Block fixieren und neuen spawnen
                grid_fix_block(&current_block);
                spawn_block();
            }
        }
        
        // ====================================================================
        // RENDERING (60 FPS)
        // ====================================================================
        
        if (current_time - last_render_time >= RENDER_INTERVAL_MS) {
            last_render_time = current_time;
            render_grid();
        }
        
        // ====================================================================
        // TASK DELAY (5ms Polling-Intervall)
        // ====================================================================
        
        vTaskDelay(pdMS_TO_TICKS(LOOP_INTERVAL_MS));
    }
}

/**
 * @brief Startet den GameLoop als FreeRTOS Task
 * 
 * Wird von app_main() aufgerufen. Erstellt einen hochprioritären Task
 * für die Spielschleife.
 */
void start_game_loop(void) {
    xTaskCreate(game_loop_task, "GameLoopTask", 4096, NULL, 5, NULL);
}
