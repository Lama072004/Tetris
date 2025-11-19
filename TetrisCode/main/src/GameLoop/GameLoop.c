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

extern MATRIX ledMatrix; // aus main.c
extern led_strip_handle_t led_strip;

static TetrisBlock current_block;
static volatile int game_over_flag = 0;
// Track previous dynamic block pixels so we can restore static pixels without clearing whole strip
static int prev_dynamic_count = 0;
static int prev_dynamic_pos[16*24][2]; // [y,x]

// Forward declaration for game over handling
static void handle_game_over(void);

static void render_grid(void) {
    // Restore previous dynamic pixels back to static colors (without clearing whole strip)
    for (int i = 0; i < prev_dynamic_count; i++) {
        int ry = prev_dynamic_pos[i][0];
        int rx = prev_dynamic_pos[i][1];
        int led_num = ledMatrix.LED_Number[ry][rx];
        if (grid[ry][rx] > 0) {
            uint8_t r,g,b;
            get_block_rgb(grid[ry][rx]-1, &r, &g, &b);
            r = (r * GAME_BRIGHTNESS_SCALE) / 255;
            g = (g * GAME_BRIGHTNESS_SCALE) / 255;
            b = (b * GAME_BRIGHTNESS_SCALE) / 255;
            led_strip_set_pixel(led_strip, led_num, r, g, b);
        } else {
            // empty -> turn off
            led_strip_set_pixel(led_strip, led_num, 0, 0, 0);
        }
    }
    prev_dynamic_count = 0;

    // Now draw current dynamic block on top of static pixels
    for(int by=0;by<4;by++){
        for(int bx=0;bx<4;bx++){
            if(current_block.shape[by][bx]){
                int gx = current_block.x + bx;
                int gy = current_block.y + by;
                if(gx>=0 && gx<GRID_WIDTH && gy>=0 && gy<GRID_HEIGHT){
                    uint8_t r,g,b;
                    get_block_rgb(current_block.color,&r,&g,&b);
                    // Apply configured brightness scale
                    r = (r * GAME_BRIGHTNESS_SCALE) / 255;
                    g = (g * GAME_BRIGHTNESS_SCALE) / 255;
                    b = (b * GAME_BRIGHTNESS_SCALE) / 255;
                    int led_num = ledMatrix.LED_Number[gy][gx];
                    led_strip_set_pixel(led_strip, led_num, r,g,b);
                    // remember to restore this pixel next frame
                    if (prev_dynamic_count < (GRID_WIDTH*GRID_HEIGHT)) {
                        prev_dynamic_pos[prev_dynamic_count][0] = gy;
                        prev_dynamic_pos[prev_dynamic_count][1] = gx;
                        prev_dynamic_count++;
                    }
                }
            }
        }
    }

    // push only the changed pixels (strip will update from its internal buffer)
    led_strip_refresh(led_strip);
}

static void spawn_block(void){
    int block_type = esp_random()%7;
    TetrisBlock candidate = blocks[block_type][0];

    // Try to find a spawn x position where the block does not immediately collide.
    // Prefer positions near center.
    int preferred = GRID_WIDTH/2 - 2;
    int found = 0;
    int best_x = preferred;
    // search offsets 0, -1, +1, -2, +2, ...
    for (int offset = 0; offset <= GRID_WIDTH; offset++){
        int dirs[2] = { -1, 1 };
        int tries[2];
        if (offset == 0) {
            tries[0] = preferred;
            int tx = tries[0];
            candidate.x = tx;
            candidate.y = 0;
            if (!grid_check_collision(&candidate)) { best_x = tx; found = 1; break; }
        } else {
            for (int d = 0; d < 2; d++){
                int tx = preferred + dirs[d]*offset;
                if (tx < 0 || tx > GRID_WIDTH-4) continue;
                candidate.x = tx;
                candidate.y = 0;
                if (!grid_check_collision(&candidate)) { best_x = tx; found = 1; break; }
            }
            if (found) break;
        }
    }

    if (!found) {
        // try one row higher (y = -1) allowing blocks to be partially above top
        for (int offset = 0; offset <= GRID_WIDTH; offset++){
            if (offset == 0) {
                candidate.x = preferred;
                candidate.y = -1;
                if (!grid_check_collision(&candidate)) { best_x = preferred; found = 1; break; }
            } else {
                for (int d = 0; d < 2; d++){
                    int tx = preferred + (d==0 ? -offset : offset);
                    if (tx < 0 || tx > GRID_WIDTH-4) continue;
                    candidate.x = tx;
                    candidate.y = -1;
                    if (!grid_check_collision(&candidate)) { best_x = tx; found = 1; break; }
                }
                if (found) break;
            }
        }
    }

    if (!found) {
        // No space to spawn -> Game Over: blink 2x red and wait for button to restart
        handle_game_over();
        // After game over, spawn_block() is called again from handle_game_over(), so return
        return;
    }

    current_block = blocks[block_type][0];
    current_block.x = best_x;
    current_block.y = (candidate.y < 0) ? -1 : 0;
    assign_block_color(&current_block, block_type);
}

static void handle_game_over(void){
    // Update highscore and display game over
    score_update_highscore();
    display_show_game_over(score_get(), score_get_highscore());
    
    // Blink red GAME_OVER_BLINK_COUNT times
    for (int blink = 0; blink < GAME_OVER_BLINK_COUNT; blink++){
        // On: all red with configured brightness
        for (int y = 0; y < GRID_HEIGHT; y++){
            for (int x = 0; x < GRID_WIDTH; x++){
                int led = ledMatrix.LED_Number[y][x];
                led_strip_set_pixel(led_strip, led, GAME_OVER_BLINK_R, GAME_OVER_BLINK_G, GAME_OVER_BLINK_B);
            }
        }
        led_strip_refresh(led_strip);
        vTaskDelay(pdMS_TO_TICKS(GAME_OVER_BLINK_ON_MS));

        // Off
        led_strip_clear(led_strip);
        led_strip_refresh(led_strip);
        vTaskDelay(pdMS_TO_TICKS(GAME_OVER_BLINK_OFF_MS));
    }

    // Set flag to indicate game over; main loop will enter wait-for-start state
    game_over_flag = 1;
}

void game_loop_task(void *pvParameters){
    speed_manager_init();
    // Start in WAIT state: do not spawn block until a button is pressed
    grid_init();
    score_init();
    speed_manager_reset();
    display_reset_and_show_hud(score_get_highscore());
    bool game_running = false;
    bool splash_shown = false; // ensure splash is shown when waiting
    bool require_release_before_splash = false;
    uint32_t wait_enable_time = 0; // until this time, button presses won't start the game
    uint32_t last_fall_time = 0;
    uint32_t last_render_time = 0;
    
    while(1){
        uint32_t current_time = xTaskGetTickCount() * portTICK_PERIOD_MS;
        // Check for emergency reset: all four buttons pressed simultaneously
        if (controls_all_buttons_pressed()) {
            // require 1 second hold to avoid accidental trigger
            vTaskDelay(pdMS_TO_TICKS(1000));
            if (controls_all_buttons_pressed()) {
                // Perform hard reset of game state and go to WAIT state
                printf("[GameLoop] Emergency reset triggered (all buttons)\n");
                grid_init();
                score_init();
                speed_manager_reset();
                // clear LED matrix and refresh
                led_strip_clear(led_strip);
                led_strip_refresh(led_strip);
                // rebuild HUD and stay in wait state (do not spawn)
                display_reset_and_show_hud(score_get_highscore());
                game_running = false;

                // Drain any pending button events
                gpio_num_t ev;
                while (controls_get_event(&ev)) {}

                // Show splash for 2 seconds while ignoring inputs
                splash_show_duration(2000);

                // After duration, drain events and wait until buttons released before looping back
                printf("[GameLoop] Draining queue after 2s splash...\n");
                int drained = 0;
                while (controls_get_event(&ev)) {
                    drained++;
                }
                printf("[GameLoop] Drained %d events from queue\n", drained);
                
                printf("[GameLoop] Waiting 500ms while continuously draining queue...\n");
                uint32_t drain_start = xTaskGetTickCount() * portTICK_PERIOD_MS;
                while ((xTaskGetTickCount() * portTICK_PERIOD_MS - drain_start) < 500) {
                    // Continuously drain queue during wait to catch any stragglers
                    while (controls_get_event(&ev)) {
                        printf("[GameLoop] Caught straggler event during drain: GPIO %d\n", ev);
                    }
                    vTaskDelay(pdMS_TO_TICKS(10));
                }
                
                printf("[GameLoop] All buttons released, now calling splash_show_waiting()...\n");
                // Show interactive splash and wait for button press to start
                splash_show_waiting();
                printf("[GameLoop] splash_show_waiting() returned - button was pressed\n");
                vTaskDelay(pdMS_TO_TICKS(50));

                // Drain any events after splash and wait for release
                while (controls_get_event(&ev)) {}
                vTaskDelay(pdMS_TO_TICKS(50));
                while (check_button_pressed(BTN_LEFT) || check_button_pressed(BTN_RIGHT) ||
                       check_button_pressed(BTN_ROTATE) || check_button_pressed(BTN_FASTER)) {
                    vTaskDelay(pdMS_TO_TICKS(20));
                }

                // Start new game
                splash_clear();
                grid_init();
                score_init();
                speed_manager_reset();
                display_reset_and_show_hud(score_get_highscore());
                spawn_block();
                game_running = true;
                last_fall_time = xTaskGetTickCount() * portTICK_PERIOD_MS;
                last_render_time = last_fall_time;
                continue; // proceed to next loop iteration
            }
        }

        // If a game over occurred, enter wait-for-start state
        if (game_over_flag) {
            game_over_flag = 0;
            game_running = false;
            // ensure HUD is visible
            display_reset_and_show_hud(score_get_highscore());

            // Drain events and show splash for 2s ignoring inputs
            gpio_num_t ev;
            while (controls_get_event(&ev)) {}
            splash_show_duration(2000);

            // After splash duration, drain queue and wait while continuously draining
            printf("[GameLoop] Draining queue after 2s splash (GameOver)...\n");
            int drained = 0;
            while (controls_get_event(&ev)) {
                drained++;
            }
            printf("[GameLoop] Drained %d events from queue\n", drained);
            
            printf("[GameLoop] Waiting 500ms while continuously draining queue (GameOver)...\n");
            uint32_t drain_start = xTaskGetTickCount() * portTICK_PERIOD_MS;
            while ((xTaskGetTickCount() * portTICK_PERIOD_MS - drain_start) < 500) {
                // Continuously drain queue during wait to catch any stragglers
                while (controls_get_event(&ev)) {
                    printf("[GameLoop] Caught straggler event during drain: GPIO %d\n", ev);
                }
                vTaskDelay(pdMS_TO_TICKS(10));
            }
            
            vTaskDelay(pdMS_TO_TICKS(50));
            while (check_button_pressed(BTN_LEFT) || check_button_pressed(BTN_RIGHT) ||
                   check_button_pressed(BTN_ROTATE) || check_button_pressed(BTN_FASTER)) {
                vTaskDelay(pdMS_TO_TICKS(20));
            }

            // Show interactive splash and wait for button press to start
            splash_show_waiting();
            vTaskDelay(pdMS_TO_TICKS(50));

            // Drain any events after splash and wait for release
            while (controls_get_event(&ev)) {}
            vTaskDelay(pdMS_TO_TICKS(50));
            while (check_button_pressed(BTN_LEFT) || check_button_pressed(BTN_RIGHT) ||
                   check_button_pressed(BTN_ROTATE) || check_button_pressed(BTN_FASTER)) {
                vTaskDelay(pdMS_TO_TICKS(20));
            }

            // Start new game
            splash_clear();
            grid_init();
            score_init();
            speed_manager_reset();
            display_reset_and_show_hud(score_get_highscore());
            spawn_block();
            game_running = true;
            last_fall_time = xTaskGetTickCount() * portTICK_PERIOD_MS;
            last_render_time = last_fall_time;
            continue;
        }

        // If not running, wait for any button press to start a new game
        if (!game_running) {
            // If reset/game-over asked for release-before-splash, wait until buttons released
            if (require_release_before_splash) {
                while (check_button_pressed(BTN_LEFT) || check_button_pressed(BTN_RIGHT) ||
                       check_button_pressed(BTN_ROTATE) || check_button_pressed(BTN_FASTER)) {
                    vTaskDelay(pdMS_TO_TICKS(20));
                }
                require_release_before_splash = false;
                // small debounce
                vTaskDelay(pdMS_TO_TICKS(50));
            }

            // Show splash screen: continuous scroll until button pressed
            // splash_show() blocks until a button is detected (after minimum duration)
            splash_show(SPLASH_DURATION_MS);

            // After splash returns, drain any pending events
            gpio_num_t ev;
            while (controls_get_event(&ev)) {}

            // Wait until all buttons are released (avoid using the press that closed splash)
            vTaskDelay(pdMS_TO_TICKS(50));
            while (check_button_pressed(BTN_LEFT) || check_button_pressed(BTN_RIGHT) ||
                   check_button_pressed(BTN_ROTATE) || check_button_pressed(BTN_FASTER)) {
                vTaskDelay(pdMS_TO_TICKS(20));
            }

            // Now wait for a fresh button press to start the game (block until event)
            controls_wait_event(&ev, portMAX_DELAY);
            // small debounce after press
            vTaskDelay(pdMS_TO_TICKS(50));

            // Start new game
            splash_clear();
            grid_init();
            score_init();
            speed_manager_reset();
            display_reset_and_show_hud(score_get_highscore());
            spawn_block();
            game_running = true;
            last_fall_time = xTaskGetTickCount() * portTICK_PERIOD_MS;
            last_render_time = last_fall_time;
            continue;
        }

        // If not running, skip the rest of the loop
        if (!game_running) {
            vTaskDelay(pdMS_TO_TICKS(LOOP_INTERVAL_MS));
            continue;
        }
        
        // Handle LEFT/RIGHT movement (every loop - responsive)
        TetrisBlock tmp;
        if(check_button_pressed(BTN_LEFT)) {
            tmp = current_block;
            tmp.x--;
            // Only grid_check_collision matters (includes corner detection)
            if(!grid_check_collision(&tmp)) current_block = tmp;
        }
        if(check_button_pressed(BTN_RIGHT)){
            tmp = current_block;
            tmp.x++;
            // Only grid_check_collision matters (includes corner detection)
            if(!grid_check_collision(&tmp)) current_block = tmp;
        }
        
        // Handle ROTATE (every loop - responsive)
        // O-Block (color=3) does not rotate
        if(check_button_pressed(BTN_ROTATE) && current_block.color != 3){  // 3 = O-Block (Quadrat)
            tmp = current_block;
            rotate_block_90(&tmp);
            // Only grid_check_collision matters (includes corner detection)
            if(!grid_check_collision(&tmp)) current_block = tmp;
        }
        
        // Handle FASTER (manual drop - every loop)
        if(check_button_pressed(BTN_FASTER)){
            tmp = current_block;
            tmp.y++;
            if(!grid_check_collision(&tmp)) current_block = tmp;
        }

        // Automatic fall (only every FALL_INTERVAL_MS milliseconds)
        uint32_t fall_interval = speed_manager_get_fall_interval();
        if (current_time - last_fall_time >= fall_interval) {
            last_fall_time = current_time;
            
            tmp = current_block;
            tmp.y++;
            if(!grid_check_collision(&tmp)){
                current_block = tmp;
            } else {
                // Block kann nicht weiter fallen -> fixieren und neuen Block spawnen
                grid_fix_block(&current_block);
                spawn_block();
            }
        }

        // Render only at fixed high frequency to eliminate flicker
        if (current_time - last_render_time >= RENDER_INTERVAL_MS) {
            last_render_time = current_time;
            render_grid();
        }
        
        // Very fast loop for responsive movement and smooth rendering
        vTaskDelay(pdMS_TO_TICKS(LOOP_INTERVAL_MS));
    }
}

void start_game_loop(void){
    xTaskCreate(game_loop_task,"GameLoopTask",4096,NULL,5,NULL);
}
