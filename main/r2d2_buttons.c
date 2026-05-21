#include "r2d2_buttons.h"
#include "r2d2_modes.h"
#include "r2d2_audio.h"
#include "driver/gpio.h"
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "esp_log.h"
#include <stdlib.h>

static const char *TAG = "R2D2_BTN";

// Debounce time in milliseconds
#define DEBOUNCE_MS  50

static void button_task(void *arg)
{
    int last_b1 = 1;
    int last_b2 = 1;
    int debug_counter = 0;

    while (1) {
        int b1 = gpio_get_level(BUTTON_1_GPIO);
        int b2 = gpio_get_level(BUTTON_2_GPIO);

        // Print raw GPIO levels every ~1 second so we can confirm reading
        if (++debug_counter >= 20) {
            ESP_LOGI(TAG, "RAW LEVELS → GPIO48=%d  GPIO47=%d  (0=pressed, 1=idle)",
                     b1, b2);
            debug_counter = 0;
        }

        // Button 1 (GPIO 48) — falling edge → play a random song! (ONLY in IDLE MODE)
        if (b1 == 0 && last_b1 == 1) {
            if (r2d2_get_mode() == MODE_AUDIO_IDLE) {
                int allowed_songs[] = {1, 3, 4, 5}; // 1=Happy Bday, 3=StarWars, 4=Mario, 5=Pokemon
                int song = allowed_songs[rand() % 4];
                ESP_LOGI(TAG, ">>> Button 1 PRESSED! Playing random song %d", song);
                r2d2_play_song(song);
            } else {
                ESP_LOGI(TAG, ">>> Button 1 IGNORED (Currently in Display Mode)");
            }
        }

        // Button 2 (GPIO 47) — falling edge → toggle Display Mode
        if (b2 == 0 && last_b2 == 1) {
            ESP_LOGI(TAG, ">>> Button 2 PRESSED! Toggling Mode...");
            r2d2_toggle_mode();
        }

        last_b1 = b1;
        last_b2 = b2;

        vTaskDelay(pdMS_TO_TICKS(50));
    }
}

void r2d2_buttons_init(void)
{
    // ── Configure Button 1 (GPIO 48) — Pull-UP, active-LOW ──
    gpio_config_t btn1_cfg = {
        .pin_bit_mask = (1ULL << BUTTON_1_GPIO),
        .mode         = GPIO_MODE_INPUT,
        .pull_up_en   = GPIO_PULLUP_ENABLE,    // Internal pull-up ON
        .pull_down_en = GPIO_PULLDOWN_DISABLE,
        .intr_type    = GPIO_INTR_DISABLE,
    };
    gpio_config(&btn1_cfg);

    // ── Configure Button 2 (GPIO 47) — Pull-UP, active-LOW ──
    gpio_config_t btn2_cfg = {
        .pin_bit_mask = (1ULL << BUTTON_2_GPIO),
        .mode         = GPIO_MODE_INPUT,
        .pull_up_en   = GPIO_PULLUP_ENABLE,    // Internal pull-up ON
        .pull_down_en = GPIO_PULLDOWN_DISABLE,
        .intr_type    = GPIO_INTR_DISABLE,
    };
    gpio_config(&btn2_cfg);

    ESP_LOGI(TAG, "Buttons ready → GPIO %d & GPIO %d (pull-up, active-LOW)",
             BUTTON_1_GPIO, BUTTON_2_GPIO);

    xTaskCreate(button_task, "btn_task", 2048, NULL, 5, NULL);
}

