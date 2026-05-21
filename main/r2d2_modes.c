#include "r2d2_modes.h"
#include "esp_log.h"

static const char *TAG = "R2D2_MODES";

volatile r2d2_mode_t current_mode = MODE_AUDIO_IDLE;

void r2d2_toggle_mode(void) {
    if (current_mode == MODE_AUDIO_IDLE) {
        current_mode = MODE_DISPLAY;
        ESP_LOGI(TAG, "Mode switched to: DISPLAY");
    } else {
        current_mode = MODE_AUDIO_IDLE;
        ESP_LOGI(TAG, "Mode switched to: AUDIO_IDLE");
    }
}

r2d2_mode_t r2d2_get_mode(void) {
    return current_mode;
}

void r2d2_set_mode(r2d2_mode_t mode) {
    current_mode = mode;
    ESP_LOGI(TAG, "Mode explicitly set to: %s", (mode == MODE_DISPLAY) ? "DISPLAY" : "AUDIO_IDLE");
}
