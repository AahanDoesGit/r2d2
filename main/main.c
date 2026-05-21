#include <stdio.h>
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "esp_log.h"
#include "bsp/esp-bsp.h"

// Include our custom modules
#include "r2d2_audio.h"
#include "r2d2_display.h"
#include "r2d2_buttons.h"
#include "r2d2_wifi.h"

static const char *TAG = "R2D2_MAIN";

void app_main(void)
{
    ESP_LOGI(TAG, "Booting Astromech OS...");

    // 1. Fire up the Display Module: Decodes JPEG + sets up pulsing eye via LVGL
    r2d2_display_init(); // Restored to enable display
    
    // 1.5 Mount the SD Card to enable Hologram Emitter GIF features
    if (bsp_sdcard_mount() != ESP_OK) {
        ESP_LOGE(TAG, "Failed to mount SD card. GIF features will be disabled.");
    } else {
        ESP_LOGI(TAG, "SD card mounted successfully at /sdcard");
    }

    // 2. Fire up the Audio Module: ES8311 codec + procedural voice task
    r2d2_audio_init();

    // 3. Fire up the Button Module: GPIO 48 = Button1, GPIO 47 = Button2
    r2d2_buttons_init();

    // 4. Fire up the Wi-Fi and Web Server (Phase 2)
    // Note: r2d2_wifi_init() blocks until connected to Wi-Fi
    r2d2_wifi_init();

    ESP_LOGI(TAG, "System Boot Complete. BSP LVGL task is running the UI.");

    // 3. BSP already handles lv_timer_handler() in its own dedicated task.
    //    app_main just needs to stay alive.
    while (1) {
        vTaskDelay(pdMS_TO_TICKS(1000));
    }
}