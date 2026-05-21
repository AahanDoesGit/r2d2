idf_component_register(SRCS "main.c" "r2d2_audio.c" "r2d2_display.c" "r2d2_buttons.c" "r2d2_modes.c" "r2d2_wifi.c" "r2d2_webserver.c"
                    INCLUDE_DIRS "."
                    REQUIRES esp_lcd lvgl esp_codec_dev esp32_p4_wifi6_touch_lcd_4_3 esp_driver_jpeg esp_driver_gpio esp_wifi esp_event nvs_flash esp_netif esp_http_server
                    EMBED_FILES "${PROJECT_DIR}/sd_card/images/R2-D2.jpg")