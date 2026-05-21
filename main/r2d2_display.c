#include "r2d2_display.h"
#include "esp_log.h"
#include "bsp/esp-bsp.h"
#include "lvgl.h"
#include "driver/jpeg_decode.h"
#include "esp_heap_caps.h"
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "r2d2_audio.h"
#include "r2d2_modes.h"
#include <time.h>
#include <string.h>
#include <stdlib.h>

static const char *TAG = "R2D2_DISPLAY";

// Symbols pointing to the embedded R2-D2.jpg in flash memory
extern const uint8_t r2d2_jpg_start[] asm("_binary_R2_D2_jpg_start");
extern const uint8_t r2d2_jpg_end[]   asm("_binary_R2_D2_jpg_end");

static lv_obj_t        *r2d2_img = NULL;
static lv_obj_t        *r2d2_eye_overlay = NULL;
static lv_image_dsc_t   r2d2_img_dsc;       // LVGL 9: lv_image_dsc_t
static uint8_t         *rgb_buf = NULL;

// Phase 4: Dynamic Display LVGL Objects
static lv_obj_t        *display_panel = NULL;
static lv_obj_t *clock_label = NULL;
static lv_obj_t *quote_heading = NULL;
static lv_obj_t *quote_label = NULL;
static lv_obj_t *display_divider = NULL;
static lv_obj_t *active_gif = NULL;

static char             current_quote[128] = "HELP ME, OBI-WAN KENOBI. YOU'RE MY ONLY HOPE.";
static SemaphoreHandle_t quote_mutex = NULL;

static bool             is_custom_quote = false;
static char             last_displayed_quote[128] = "";

// Star Wars themed quotes for the display panel (expanded)
static const char* default_quotes[] = {
    "HELP ME, OBI-WAN KENOBI. YOU'RE MY ONLY HOPE.",
    "MAY THE FORCE BE WITH YOU.",
    "BEEP BOOP BEEP BEEP!",
    "I HAVE A BAD FEELING ABOUT THIS.",
    "THAT'S NO MOON. IT'S A SPACE STATION.",
    "DO. OR DO NOT. THERE IS NO TRY.",
    "IT'S A TRAP!",
    "THIS IS THE WAY.",
    "NEVER TELL ME THE ODDS.",
    "I AM YOUR FATHER.",
    "THE FORCE WILL BE WITH YOU. ALWAYS.",
    "I FIND YOUR LACK OF FAITH DISTURBING."
};
#define NUM_DEFAULT_QUOTES (sizeof(default_quotes)/sizeof(default_quotes[0]))

static uint32_t last_quote_update_tick = 0;
static int current_quote_idx = 0;

void r2d2_display_set_quote(const char *quote) {
    if (quote_mutex == NULL) {
        quote_mutex = xSemaphoreCreateMutex();
    }
    if (xSemaphoreTake(quote_mutex, pdMS_TO_TICKS(100)) == pdTRUE) {
        strncpy(current_quote, quote, sizeof(current_quote) - 1);
        current_quote[sizeof(current_quote) - 1] = '\0';
        xSemaphoreGive(quote_mutex);
    }
}

void r2d2_display_get_quote(char *buf, size_t max_len) {
    if (quote_mutex == NULL) {
        quote_mutex = xSemaphoreCreateMutex();
    }
    if (xSemaphoreTake(quote_mutex, pdMS_TO_TICKS(100)) == pdTRUE) {
        strncpy(buf, current_quote, max_len - 1);
        buf[max_len - 1] = '\0';
        xSemaphoreGive(quote_mutex);
    } else {
        strncpy(buf, "BEEP BOOP!", max_len - 1);
        buf[max_len - 1] = '\0';
    }
}

// Callback to animate the quote scrolling vertically with a 3D-like fading perspective illusion
static void quote_scroll_anim_cb(void * var, int32_t v)
{
    lv_obj_t *obj = (lv_obj_t *)var;
    lv_obj_set_y(obj, v);
    
    // v goes from 480 down to 230
    int opacity = 255;
    if (v > 430) {
        // Fade in at the bottom (v: 480 -> 430 => opacity: 0 -> 255)
        opacity = (480 - v) * 255 / 50;
    } else if (v < 330) {
        // Fade out at the top (v: 330 -> 230 => opacity: 255 -> 0)
        opacity = (v - 230) * 255 / 100;
    }
    if (opacity < 0) opacity = 0;
    if (opacity > 255) opacity = 255;
    
    lv_obj_set_style_text_opa(obj, opacity, LV_PART_MAIN);
}

// Start or restart the scroll animation with a specified duration
static void start_quote_scroll_animation(uint32_t duration_ms)
{
    if (quote_label == NULL) return;
    
    // Delete any active scroll animation on quote_label
    lv_anim_delete(quote_label, quote_scroll_anim_cb);
    
    lv_anim_t a;
    lv_anim_init(&a);
    lv_anim_set_var(&a, quote_label);
    lv_anim_set_exec_cb(&a, quote_scroll_anim_cb);
    lv_anim_set_values(&a, 480, 230); // Scroll upwards from y=480 to y=230 (top)
    lv_anim_set_duration(&a, duration_ms);
    lv_anim_set_repeat_count(&a, 1); // Run once per quote update/cycle
    lv_anim_start(&a);
}

// Force-refresh/cycle the quote on the display panel (called from web server or local events)
void r2d2_display_refresh_quote(void) {
    bsp_display_lock(-1);
    if (quote_label != NULL) {
        current_quote_idx = (current_quote_idx + 1) % NUM_DEFAULT_QUOTES;
        lv_label_set_text(quote_label, default_quotes[current_quote_idx]);
        
        // Update the current_quote shared string
        r2d2_display_set_quote(default_quotes[current_quote_idx]);
        
        // Update the last update tick so it doesn't cycle immediately
        last_quote_update_tick = xTaskGetTickCount();
        
        // Start scroll animation
        start_quote_scroll_animation(8000);
        ESP_LOGI(TAG, "Quote force-refreshed to: '%s'", default_quotes[current_quote_idx]);
    }
    bsp_display_unlock();
}

// LVGL timer callback: syncs red dot opacity and handles mode switching
static void r2d2_ui_sync_cb(lv_timer_t *timer)
{
    if (r2d2_eye_overlay == NULL || r2d2_img == NULL || display_panel == NULL) return;

    r2d2_mode_t mode = r2d2_get_mode();
    static r2d2_mode_t last_mode = MODE_AUDIO_IDLE;

    if (mode == MODE_DISPLAY) {
        // Hide the R2-D2 face and show the Dynamic Display panel
        lv_obj_add_flag(r2d2_img, LV_OBJ_FLAG_HIDDEN);
        lv_obj_add_flag(r2d2_eye_overlay, LV_OBJ_FLAG_HIDDEN);
        lv_obj_clear_flag(display_panel, LV_OBJ_FLAG_HIDDEN);
        
        // Detect transition into DISPLAY mode
        if (last_mode != MODE_DISPLAY) {
            if (!is_custom_quote) {
                last_quote_update_tick = 0; // Force immediate quote update
            }
        }

        // 1. Update the Clock label using local system time
        time_t now;
        struct tm timeinfo;
        time(&now);
        localtime_r(&now, &timeinfo);
        
        char time_str[32];
        if (timeinfo.tm_year < 75) {
            uint32_t sec = xTaskGetTickCount() / configTICK_RATE_HZ;
            snprintf(time_str, sizeof(time_str), "%02lu:%02lu:%02lu", 
                     (sec / 3600) % 24, (sec / 60) % 60, sec % 60);
        } else {
            strftime(time_str, sizeof(time_str), "%H:%M:%S", &timeinfo);
        }
        lv_label_set_text(clock_label, time_str);
        
        // 2. Cycle quotes periodically or display custom active quote
        uint32_t current_tick = xTaskGetTickCount();
        
        char shared_q[128];
        r2d2_display_get_quote(shared_q, sizeof(shared_q));
        
        // If it changed from outside (e.g. from web /api/quote)
        if (strcmp(shared_q, last_displayed_quote) != 0) {
            strncpy(last_displayed_quote, shared_q, sizeof(last_displayed_quote) - 1);
            last_displayed_quote[sizeof(last_displayed_quote) - 1] = '\0';
            is_custom_quote = true;
            lv_label_set_text(quote_label, last_displayed_quote);
            last_quote_update_tick = current_tick;
            
            // Start 20-second custom quote scroll animation!
            start_quote_scroll_animation(20000);
        }
        
        // If not custom quote, cycle default quotes
        if (!is_custom_quote) {
            if (current_tick - last_quote_update_tick > pdMS_TO_TICKS(8000) || last_quote_update_tick == 0) {
                last_quote_update_tick = current_tick;
                current_quote_idx = (current_quote_idx + 1) % NUM_DEFAULT_QUOTES;
                lv_label_set_text(quote_label, default_quotes[current_quote_idx]);
                strncpy(last_displayed_quote, default_quotes[current_quote_idx], sizeof(last_displayed_quote) - 1);
                last_displayed_quote[sizeof(last_displayed_quote) - 1] = '\0';
                
                // Keep the shared variable in sync
                r2d2_display_set_quote(default_quotes[current_quote_idx]);
                
                // Start 8-second standard quote scroll animation!
                start_quote_scroll_animation(8000);
            }
        } else {
            // Revert back to auto cycling after 20 seconds
            if (current_tick - last_quote_update_tick > pdMS_TO_TICKS(20000)) {
                is_custom_quote = false;
                last_quote_update_tick = 0; // Trigger cycle immediately
                
                // Hide GIF if it was playing
                if (active_gif != NULL) {
                    lv_obj_add_flag(active_gif, LV_OBJ_FLAG_HIDDEN);
                    lv_gif_set_src(active_gif, NULL); // Stop and release file descriptor/framebuffer!
                }
                lv_obj_clear_flag(clock_label, LV_OBJ_FLAG_HIDDEN);
                lv_obj_clear_flag(quote_heading, LV_OBJ_FLAG_HIDDEN);
                lv_obj_clear_flag(display_divider, LV_OBJ_FLAG_HIDDEN);
                lv_obj_clear_flag(quote_label, LV_OBJ_FLAG_HIDDEN);
            }
        }
    } else {
        // Detect transition out of DISPLAY mode
        if (last_mode == MODE_DISPLAY) {
            if (active_gif != NULL) {
                lv_obj_add_flag(active_gif, LV_OBJ_FLAG_HIDDEN);
                lv_gif_set_src(active_gif, NULL); // Stop and release file descriptor/framebuffer!
            }
            lv_obj_clear_flag(clock_label, LV_OBJ_FLAG_HIDDEN);
            lv_obj_clear_flag(quote_heading, LV_OBJ_FLAG_HIDDEN);
            lv_obj_clear_flag(display_divider, LV_OBJ_FLAG_HIDDEN);
            lv_obj_clear_flag(quote_label, LV_OBJ_FLAG_HIDDEN);
            
            is_custom_quote = false;
            last_quote_update_tick = 0;
        }

        // Show R2-D2 body image with blinking red dot
        lv_obj_clear_flag(r2d2_img, LV_OBJ_FLAG_HIDDEN);
        lv_obj_clear_flag(r2d2_eye_overlay, LV_OBJ_FLAG_HIDDEN);
        lv_obj_add_flag(display_panel, LV_OBJ_FLAG_HIDDEN);
        
        // Blink red dot when speaking, match image when idle
        if (r2d2_is_speaking) {
            uint32_t blink_tick = xTaskGetTickCount();
            bool blink_on = ((blink_tick / pdMS_TO_TICKS(250)) % 2) == 0;
            lv_obj_set_style_bg_color(r2d2_eye_overlay,
                blink_on ? lv_color_make(255, 100, 90) : lv_color_make(120, 20, 15), LV_PART_MAIN);
        } else {
            // Match the image's red circle color exactly
            lv_obj_set_style_bg_color(r2d2_eye_overlay, lv_color_make(244, 66, 54), LV_PART_MAIN);
        }
    }
    last_mode = mode;
}

void r2d2_display_init(void)
{
    ESP_LOGI(TAG, "Initializing R2-D2 Display Subsystem...");

    // 1. Initialize Waveshare BSP display (starts internal LVGL 9 task)
    bsp_display_start();
    bsp_display_backlight_on();
    ESP_LOGI(TAG, "MIPI DSI Panel and Backlight Active.");

    // Give the LVGL task time to fully start before acquiring the lock
    vTaskDelay(pdMS_TO_TICKS(300));

    // 2. Decode embedded JPEG using hardware JPEG decoder
    size_t jpg_len = r2d2_jpg_end - r2d2_jpg_start;
    ESP_LOGI(TAG, "Embedded JPEG: %u bytes at %p", (unsigned)jpg_len, r2d2_jpg_start);

    // Copy JPEG from flash (XIP) to DMA-accessible PSRAM input buffer
    jpeg_decode_memory_alloc_cfg_t in_cfg = {
        .buffer_direction = JPEG_DEC_ALLOC_INPUT_BUFFER,
    };
    size_t jpg_ram_size = 0;
    uint8_t *jpg_ram = jpeg_alloc_decoder_mem(jpg_len, &in_cfg, &jpg_ram_size);
    if (!jpg_ram) {
        ESP_LOGE(TAG, "Failed to alloc JPEG input buffer (%u bytes)", (unsigned)jpg_len);
        return;
    }
    memcpy(jpg_ram, r2d2_jpg_start, jpg_len);
    ESP_LOGI(TAG, "JPEG copied to PSRAM input buffer (%u bytes)", (unsigned)jpg_ram_size);

    // Parse JPEG header (from RAM copy — HW driver can't read flash XIP directly)
    jpeg_decode_picture_info_t pic_info = {0};
    if (jpeg_decoder_get_info(jpg_ram, jpg_len, &pic_info) != ESP_OK) {
        ESP_LOGE(TAG, "Failed to read JPEG header");
        heap_caps_free(jpg_ram);
        return;
    }
    ESP_LOGI(TAG, "JPEG: %lux%lu px", pic_info.width, pic_info.height);

    // HW decoder pads output to 16-pixel alignment — use padded width as stride
    uint32_t padded_w = (pic_info.width  + 15) & ~15;
    uint32_t padded_h = (pic_info.height + 15) & ~15;
    size_t   out_size = padded_w * padded_h * 2;   // RGB565 = 2 bytes/pixel

    jpeg_decode_memory_alloc_cfg_t out_cfg = {
        .buffer_direction = JPEG_DEC_ALLOC_OUTPUT_BUFFER,
    };
    size_t actual_size = 0;
    rgb_buf = jpeg_alloc_decoder_mem(out_size, &out_cfg, &actual_size);
    if (!rgb_buf) {
        ESP_LOGE(TAG, "Failed to alloc JPEG output buffer (%u bytes)", (unsigned)out_size);
        heap_caps_free(jpg_ram);
        return;
    }

    // Create HW JPEG decoder engine and decode to RGB565
    jpeg_decode_engine_cfg_t engine_cfg = { .intr_priority = 0, .timeout_ms = 1000 };
    jpeg_decoder_handle_t dec = NULL;
    if (jpeg_new_decoder_engine(&engine_cfg, &dec) != ESP_OK) {
        ESP_LOGE(TAG, "Failed to create HW JPEG decoder engine");
        heap_caps_free(jpg_ram);
        heap_caps_free(rgb_buf);
        return;
    }

    jpeg_decode_cfg_t dec_cfg = {
        .output_format = JPEG_DECODE_OUT_FORMAT_RGB565,
        .rgb_order     = JPEG_DEC_RGB_ELEMENT_ORDER_BGR,   // LVGL native byte order
        .conv_std      = JPEG_YUV_RGB_CONV_STD_BT601,
    };
    uint32_t decoded_size = 0;
    esp_err_t ret = jpeg_decoder_process(dec, &dec_cfg,
                                         jpg_ram, jpg_len,
                                         rgb_buf, actual_size, &decoded_size);
    jpeg_del_decoder_engine(dec);
    heap_caps_free(jpg_ram);

    if (ret != ESP_OK) {
        ESP_LOGE(TAG, "JPEG decode failed: %s", esp_err_to_name(ret));
        heap_caps_free(rgb_buf);
        return;
    }
    ESP_LOGI(TAG, "JPEG decoded OK: %lu bytes of RGB565", decoded_size);

    // 3. Build LVGL 9 image descriptor
    //    stride = padded_w * 2 bytes/pixel  (critical to avoid row-shift artifacts)
    memset(&r2d2_img_dsc, 0, sizeof(r2d2_img_dsc));
    r2d2_img_dsc.header.magic  = LV_IMAGE_HEADER_MAGIC;
    r2d2_img_dsc.header.cf     = LV_COLOR_FORMAT_RGB565;
    r2d2_img_dsc.header.w      = pic_info.width;
    r2d2_img_dsc.header.h      = pic_info.height;
    r2d2_img_dsc.header.stride = (uint16_t)(padded_w * 2);
    r2d2_img_dsc.data_size     = actual_size;
    r2d2_img_dsc.data          = rgb_buf;

    // 4. Build LVGL 9 UI — must be inside bsp_display_lock()
    bsp_display_lock(-1);

    lv_obj_t *screen = lv_scr_act();

    // Dark background matching R2-D2 body
    lv_obj_set_style_bg_color(screen, lv_color_make(0, 0, 0), LV_PART_MAIN);
    lv_obj_set_style_bg_opa(screen, LV_OPA_COVER, LV_PART_MAIN);

    // Display the R2-D2 body image full-screen
    r2d2_img = lv_image_create(screen);
    lv_image_set_src(r2d2_img, &r2d2_img_dsc);
    lv_obj_center(r2d2_img);

    // Red dot overlay on top of the image's red circle
    r2d2_eye_overlay = lv_obj_create(screen);
    lv_obj_set_size(r2d2_eye_overlay, 330, 330);
    lv_obj_align(r2d2_eye_overlay, LV_ALIGN_CENTER, -3, -176);
    lv_obj_set_style_radius(r2d2_eye_overlay, LV_RADIUS_CIRCLE, LV_PART_MAIN);
    lv_obj_set_style_bg_color(r2d2_eye_overlay, lv_color_make(244, 66, 54), LV_PART_MAIN);
    lv_obj_set_style_bg_opa(r2d2_eye_overlay, LV_OPA_COVER, LV_PART_MAIN); 
    lv_obj_set_style_opa(r2d2_eye_overlay, LV_OPA_COVER, LV_PART_MAIN);
    lv_obj_set_style_border_width(r2d2_eye_overlay, 0, LV_PART_MAIN);
    lv_obj_clear_flag(r2d2_eye_overlay, LV_OBJ_FLAG_SCROLLABLE);

    // Initialize the quote mutex
    if (quote_mutex == NULL) {
        quote_mutex = xSemaphoreCreateMutex();
    }

    // 5. Create the Dynamic Display Panel (hidden by default)
    display_panel = lv_obj_create(screen);
    lv_obj_set_size(display_panel, 480, 800);
    lv_obj_center(display_panel);
    lv_obj_set_style_bg_color(display_panel, lv_color_make(0, 0, 0), LV_PART_MAIN); // Star Wars Pure Black Background!
    lv_obj_set_style_bg_opa(display_panel, LV_OPA_COVER, LV_PART_MAIN);
    lv_obj_set_style_border_width(display_panel, 0, LV_PART_MAIN);
    lv_obj_add_flag(display_panel, LV_OBJ_FLAG_HIDDEN);

    // Create a beautiful premium Starfield Background on display_panel (40 random stars)
    for (int i = 0; i < 40; i++) {
        lv_obj_t *star = lv_obj_create(display_panel);
        int size = (rand() % 2) + 1; // 1 or 2 pixels
        lv_obj_set_size(star, size, size);
        lv_obj_set_pos(star, rand() % 480, rand() % 800);
        lv_obj_set_style_bg_color(star, lv_color_make(255, 255, 255), LV_PART_MAIN);
        lv_obj_set_style_bg_opa(star, (rand() % 150) + 105, LV_PART_MAIN); // Random star brightness
        lv_obj_set_style_border_width(star, 0, LV_PART_MAIN);
        lv_obj_set_style_radius(star, LV_RADIUS_CIRCLE, LV_PART_MAIN);
        lv_obj_clear_flag(star, LV_OBJ_FLAG_SCROLLABLE);
    }

    // Digital Clock Label (Montserrat 48, glowing Star Wars Gold)
    clock_label = lv_label_create(display_panel);
    lv_obj_align(clock_label, LV_ALIGN_TOP_MID, 0, 80);
    lv_obj_set_style_text_font(clock_label, &lv_font_montserrat_48, LV_PART_MAIN);
    lv_obj_set_style_text_color(clock_label, lv_color_make(255, 232, 31), LV_PART_MAIN); // Star Wars gold/yellow (#ffe81f)

    // Dialogue Translation Matrix Heading
    quote_heading = lv_label_create(display_panel);
    lv_obj_align(quote_heading, LV_ALIGN_TOP_MID, 0, 160);
    lv_obj_set_style_text_font(quote_heading, &lv_font_montserrat_14, LV_PART_MAIN);
    lv_obj_set_style_text_color(quote_heading, lv_color_make(255, 232, 31), LV_PART_MAIN); // Star Wars gold/yellow (#ffe81f)
    lv_obj_set_style_text_opa(quote_heading, 180, LV_PART_MAIN); // Semi-transparent for elegance
    lv_label_set_text(quote_heading, "--- DIALOGUE TRANSLATION MATRIX ---");

    // Golden decorative divider line
    display_divider = lv_obj_create(display_panel);
    lv_obj_set_size(display_divider, 360, 2);
    lv_obj_align(display_divider, LV_ALIGN_TOP_MID, 0, 190);
    lv_obj_set_style_bg_color(display_divider, lv_color_make(255, 232, 31), LV_PART_MAIN); // Star Wars gold/yellow
    lv_obj_set_style_bg_opa(display_divider, LV_OPA_COVER, LV_PART_MAIN);
    lv_obj_set_style_border_width(display_divider, 0, LV_PART_MAIN);

    // Quotes Text Label (Montserrat 24, managed by vertical perspective scrolling animation)
    quote_label = lv_label_create(display_panel);
    lv_obj_set_width(quote_label, 400);
    lv_label_set_long_mode(quote_label, LV_LABEL_LONG_WRAP);
    lv_obj_set_style_text_align(quote_label, LV_TEXT_ALIGN_CENTER, LV_PART_MAIN);
    lv_obj_align(quote_label, LV_ALIGN_TOP_MID, 0, 0); // Position Y will be controlled by animation
    lv_obj_set_style_text_font(quote_label, &lv_font_montserrat_24, LV_PART_MAIN);
    lv_obj_set_style_text_color(quote_label, lv_color_make(255, 232, 31), LV_PART_MAIN); // Star Wars Gold (#ffe81f)
    lv_label_set_text(quote_label, "AWAITING PROJECTED INSTRUCTIONS...");

    // Create the GIF widget but hide it initially
    active_gif = lv_gif_create(display_panel);
    lv_gif_set_color_format(active_gif, LV_COLOR_FORMAT_RGB565);
    lv_obj_set_size(active_gif, 480, 800);
    lv_obj_center(active_gif);
    lv_obj_set_style_pad_all(active_gif, 0, LV_PART_MAIN);
    lv_obj_set_style_border_width(active_gif, 0, LV_PART_MAIN);
    lv_image_set_inner_align(active_gif, LV_IMAGE_ALIGN_CONTAIN);
    lv_obj_add_flag(active_gif, LV_OBJ_FLAG_HIDDEN);

    // Create a 50ms LVGL timer to constantly sync the UI with state
    lv_timer_create(r2d2_ui_sync_cb, 50, NULL);

    bsp_display_unlock();

    ESP_LOGI(TAG, "R2-D2 display ready! (LVGL 9, Waveshare BSP, 480x800)");
}

void r2d2_display_play_gif(const char *filename)
{
    ESP_LOGI(TAG, "Playing GIF: %s", filename);

    // Reset shared state so that re-triggering the same GIF works
    is_custom_quote = true;
    last_displayed_quote[0] = '\0';

    // Switch to DISPLAY mode so the display panel is visible
    r2d2_set_mode(MODE_DISPLAY);

    // Show "NOW PLAYING" message on quote label
    char msg[128];
    snprintf(msg, sizeof(msg), "NOW PLAYING: %s", filename);
    r2d2_display_set_quote(msg);

    bsp_display_lock(-1);

    // Hide clock, matrix text, and divider to make room for the GIF
    lv_obj_add_flag(clock_label, LV_OBJ_FLAG_HIDDEN);
    lv_obj_add_flag(quote_heading, LV_OBJ_FLAG_HIDDEN);
    lv_obj_add_flag(display_divider, LV_OBJ_FLAG_HIDDEN);
    lv_obj_add_flag(quote_label, LV_OBJ_FLAG_HIDDEN);

    // Load the GIF using LVGL filesystem drive 'S:' mapped to /sdcard
    if (active_gif != NULL) {
        static char gif_path[128];
        snprintf(gif_path, sizeof(gif_path), "S:/gifs/%s", filename);
        lv_gif_set_src(active_gif, gif_path);
        bool loaded = lv_gif_is_loaded(active_gif);
        ESP_LOGI(TAG, "GIF path set: '%s', loaded successfully: %s", gif_path, loaded ? "YES" : "NO");
        lv_obj_clear_flag(active_gif, LV_OBJ_FLAG_HIDDEN);
    }

    last_quote_update_tick = xTaskGetTickCount();
    
    bsp_display_unlock();
}