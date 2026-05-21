#include <stdio.h>
#include <stdlib.h>
#include <math.h>
#include <string.h>
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "freertos/semphr.h"
#include "esp_log.h"
#include "driver/i2s_std.h" 
#include "bsp/esp-bsp.h" 
#include "esp_codec_dev.h"
#include "r2d2_audio.h" // Pull in our own header

static const char *TAG = "R2D2_AUDIO";
static esp_codec_dev_handle_t speaker_dev = NULL;
static SemaphoreHandle_t speaker_mutex = NULL;
#define SAMPLE_RATE     (22050)

volatile bool r2d2_is_speaking = false;

// Helper: Generates and plays a raw tone
static void play_astromech_beep(int start_freq, int end_freq, int duration_ms) {
    if (speaker_mutex) {
        xSemaphoreTake(speaker_mutex, portMAX_DELAY);
    }
    int num_samples = (SAMPLE_RATE * duration_ms) / 1000;
    int16_t *sample_buffer = malloc(num_samples * sizeof(int16_t));
    
    if (sample_buffer == NULL) {
        ESP_LOGE(TAG, "Audio buffer allocation failed!");
        if (speaker_mutex) {
            xSemaphoreGive(speaker_mutex);
        }
        return;
    }

    for (int i = 0; i < num_samples; i++) {
        double t = (double)i / SAMPLE_RATE;
        double progress = (double)i / num_samples;
        double current_freq = start_freq + (end_freq - start_freq) * progress;
        // Add slight amplitude envelope to avoid clicks
        double env = (i < 100) ? (double)i / 100.0 : (i > num_samples - 100) ? (double)(num_samples - i) / 100.0 : 1.0;
        sample_buffer[i] = (int16_t)(10000 * env * sin(2 * M_PI * current_freq * t));
    }

    r2d2_is_speaking = true;
    if (speaker_dev) {
        esp_codec_dev_write(speaker_dev, sample_buffer, num_samples * sizeof(int16_t));
    }
    r2d2_is_speaking = false;
    free(sample_buffer);
    if (speaker_mutex) {
        xSemaphoreGive(speaker_mutex);
    }
}

// Helper: play a silence gap
static void play_gap(int duration_ms) {
    if (speaker_mutex) {
        xSemaphoreTake(speaker_mutex, portMAX_DELAY);
    }
    int num_samples = (SAMPLE_RATE * duration_ms) / 1000;
    int16_t *buf = calloc(num_samples, sizeof(int16_t));
    if (buf && speaker_dev) {
        esp_codec_dev_write(speaker_dev, buf, num_samples * sizeof(int16_t));
    }
    free(buf);
    if (speaker_mutex) {
        xSemaphoreGive(speaker_mutex);
    }
}

// ── Note frequencies (Hz) ──
#define NOTE_G3  196
#define NOTE_GS3 208
#define NOTE_A3  220
#define NOTE_AS3 233
#define NOTE_B3  247
#define NOTE_C4  262
#define NOTE_CS4 277
#define NOTE_D4  294
#define NOTE_DS4 311
#define NOTE_E4  330
#define NOTE_F4  349
#define NOTE_FS4 370
#define NOTE_G4  392
#define NOTE_GS4 415
#define NOTE_A4  440
#define NOTE_AS4 466
#define NOTE_B4  494
#define NOTE_C5  523
#define NOTE_CS5 554
#define NOTE_D5  587
#define NOTE_DS5 622
#define NOTE_E5  659
#define NOTE_F5  698
#define NOTE_FS5 740
#define NOTE_G5  784
#define NOTE_GS5 831
#define NOTE_A5  880
#define NOTE_AS5 932
#define NOTE_B5  988
#define NOTE_C6  1047

// Struct for a note
typedef struct { int freq; int dur; } Note;

// ── Song 0: Back in Black intro riff (AC/DC) — R2-D2 style ──
static void play_back_in_black(void) {
    ESP_LOGI(TAG, "Playing: Back in Black (R2-D2 edition)");
    Note riff[] = {
        {NOTE_E4, 120}, {0, 60}, {NOTE_E4, 80}, {0, 40},
        {NOTE_E4, 120}, {0, 60}, {NOTE_D4, 80}, {0, 40},
        {NOTE_E4, 200}, {0, 80},
        {NOTE_A3, 120}, {0, 60}, {NOTE_A3, 80}, {0, 40},
        {NOTE_A3, 120}, {0, 60}, {NOTE_G4, 80}, {0, 40},
        {NOTE_A3, 200}, {0, 100},
        {NOTE_E4, 120}, {0, 60}, {NOTE_E4, 80}, {0, 40},
        {NOTE_D4, 120}, {0, 60}, {NOTE_E4, 80}, {0, 40},
        {NOTE_B3, 300}, {0, 120},
    };
    for (int i = 0; i < sizeof(riff)/sizeof(riff[0]); i++) {
        if (riff[i].freq == 0) play_gap(riff[i].dur);
        else play_astromech_beep(riff[i].freq, riff[i].freq + 30, riff[i].dur);
    }
}

// ── Song 1: Happy Birthday ──
static void play_happy_birthday(void) {
    ESP_LOGI(TAG, "Playing: Happy Birthday (R2-D2 edition)");
    Note song[] = {
        {NOTE_C4, 200}, {0, 50}, {NOTE_C4, 100}, {0, 50},
        {NOTE_D4, 300}, {0, 50}, {NOTE_C4, 300}, {0, 50},
        {NOTE_F4, 300}, {0, 50}, {NOTE_E4, 500}, {0, 100},
        {NOTE_C4, 200}, {0, 50}, {NOTE_C4, 100}, {0, 50},
        {NOTE_D4, 300}, {0, 50}, {NOTE_C4, 300}, {0, 50},
        {NOTE_G4, 300}, {0, 50}, {NOTE_F4, 500}, {0, 100},
        {NOTE_C4, 200}, {0, 50}, {NOTE_C4, 100}, {0, 50},
        {NOTE_C5, 300}, {0, 50}, {NOTE_A4, 300}, {0, 50},
        {NOTE_F4, 300}, {0, 50}, {NOTE_E4, 300}, {0, 50},
        {NOTE_D4, 400}, {0, 80},
        {NOTE_B4, 200}, {0, 50}, {NOTE_B4, 100}, {0, 50},
        {NOTE_A4, 300}, {0, 50}, {NOTE_F4, 300}, {0, 50},
        {NOTE_G4, 300}, {0, 50}, {NOTE_F4, 600},
    };
    for (int i = 0; i < sizeof(song)/sizeof(song[0]); i++) {
        if (song[i].freq == 0) play_gap(song[i].dur);
        else play_astromech_beep(song[i].freq, song[i].freq + 20, song[i].dur);
    }
}

// ── Song 2: Never Gonna Give You Up (Rick Astley) — the classic rickroll ──
static void play_never_gonna(void) {
    ESP_LOGI(TAG, "Playing: Never Gonna Give You Up (R2-D2 Rickroll edition)");
    Note song[] = {
        {NOTE_D5, 150}, {0, 50}, {NOTE_E5, 150}, {0, 50},
        {NOTE_A4, 150}, {0, 50}, {NOTE_E5, 200}, {0, 50},
        {NOTE_F5, 200}, {0, 50}, {NOTE_E5, 300}, {0, 80},
        {NOTE_D5, 150}, {0, 50}, {NOTE_E5, 150}, {0, 50},
        {NOTE_A4, 400}, {0, 100},
        {NOTE_D5, 150}, {0, 50}, {NOTE_E5, 150}, {0, 50},
        {NOTE_A4, 150}, {0, 50}, {NOTE_G5, 200}, {0, 50},
        {NOTE_E5, 150}, {0, 50}, {NOTE_D5, 150}, {0, 50},
        {NOTE_A4, 150}, {0, 50}, {NOTE_B4, 150}, {0, 50},
        {NOTE_A4, 400}, {0, 100},
        {NOTE_D5, 150}, {0, 50}, {NOTE_E5, 150}, {0, 50},
        {NOTE_A4, 150}, {0, 50}, {NOTE_E5, 200}, {0, 50},
        {NOTE_F5, 200}, {0, 50}, {NOTE_G5, 300}, {0, 80},
    };
    for (int i = 0; i < sizeof(song)/sizeof(song[0]); i++) {
        if (song[i].freq == 0) play_gap(song[i].dur);
        else play_astromech_beep(song[i].freq, song[i].freq + 15, song[i].dur);
    }
}

// ── Song 3: Star Wars Main Theme ──
static void play_star_wars(void) {
    ESP_LOGI(TAG, "Playing: Star Wars Theme");
    Note song[] = {
        {NOTE_D4, 150}, {NOTE_D4, 150}, {NOTE_D4, 150}, 
        {NOTE_G4, 900}, {NOTE_D5, 900}, 
        {NOTE_C5, 150}, {NOTE_B4, 150}, {NOTE_A4, 150}, {NOTE_G5, 900}, {NOTE_D5, 450}, 
        {NOTE_C5, 150}, {NOTE_B4, 150}, {NOTE_A4, 150}, {NOTE_G5, 900}, {NOTE_D5, 450},
        {NOTE_C5, 150}, {NOTE_B4, 150}, {NOTE_C5, 150}, {NOTE_A4, 900}
    };
    for (int i = 0; i < sizeof(song)/sizeof(song[0]); i++) {
        if (song[i].freq == 0) play_gap(song[i].dur);
        else { play_astromech_beep(song[i].freq, song[i].freq, song[i].dur); play_gap(50); }
    }
}

// ── Song 4: Super Mario Bros Theme ──
static void play_mario(void) {
    ESP_LOGI(TAG, "Playing: Super Mario Theme");
    Note song[] = {
        {NOTE_E5, 150}, {0, 150}, {NOTE_E5, 150}, {0, 150}, {NOTE_C5, 150}, {NOTE_E5, 150}, {0, 150}, 
        {NOTE_G5, 300}, {0, 300}, {NOTE_G4, 300}, {0, 300},
        {NOTE_C5, 300}, {0, 150}, {NOTE_G4, 300}, {0, 150}, {NOTE_E4, 300}, {0, 150},
        {NOTE_A4, 150}, {0, 150}, {NOTE_B4, 150}, {0, 150}, {NOTE_AS4, 150}, {NOTE_A4, 150}, {0, 150}
    };
    for (int i = 0; i < sizeof(song)/sizeof(song[0]); i++) {
        if (song[i].freq == 0) play_gap(song[i].dur);
        else play_astromech_beep(song[i].freq, song[i].freq, song[i].dur);
    }
}

// ── Song 5: Pokemon Diamond Lake Theme (Verity/Valor/Acuity) ──
static void play_pokemon(void) {
    ESP_LOGI(TAG, "Playing: Pokemon Diamond Lake Theme");
    // Slow, ambient piano arpeggio signature of the Sinnoh Lakes
    Note song[] = {
        {NOTE_A4, 400}, {NOTE_F4, 400}, {NOTE_C5, 800}, 
        {NOTE_A4, 400}, {NOTE_F4, 400}, {NOTE_C5, 800},
        {NOTE_G4, 400}, {NOTE_E4, 400}, {NOTE_C5, 800},
        {NOTE_G4, 400}, {NOTE_E4, 400}, {NOTE_C5, 800},
        {NOTE_A4, 400}, {NOTE_F4, 400}, {NOTE_C5, 800}
    };
    for (int i = 0; i < sizeof(song)/sizeof(song[0]); i++) {
        if (song[i].freq == 0) play_gap(song[i].dur);
        else play_astromech_beep(song[i].freq, song[i].freq, song[i].dur);
    }
}

// Public: called by button handler — plays chosen song in a one-shot task
static void song_task(void *arg) {
    int song_id = (int)(intptr_t)arg;
    switch (song_id) {
        case 0: play_back_in_black();  break;
        case 1: play_happy_birthday(); break;
        case 2: play_never_gonna();    break;
        case 3: play_star_wars();      break;
        case 4: play_mario();          break;
        case 5: play_pokemon();        break;
        default: break;
    }
    vTaskDelete(NULL); // task cleans itself up when done
}

void r2d2_play_song(int song_id) {
    // Run in a separate task so it doesn't block the button handler
    xTaskCreatePinnedToCore(song_task, "song_task", 8192, (void *)(intptr_t)song_id, 6, NULL, 1);
}


// Background Task: The "Brain" of the voice
static void r2d2_voice_task(void *pvParameters) {
    while (1) {
        ESP_LOGI(TAG, "Waiting 20 seconds...");
        vTaskDelay(pdMS_TO_TICKS(20000)); 

        ESP_LOGI(TAG, "Speaking!");
        
        int phrase_duration = 0;
        // Speak for 5 seconds
        while (phrase_duration < 5000) {
            int base_pitch = 600 + (rand() % 1200); 
            int pitch_bend = (rand() % 600) - 300; 
            int beep_type = rand() % 3;
            int duration = 80 + (rand() % 300); 
            
            if (beep_type == 0) {
                play_astromech_beep(base_pitch, base_pitch, duration);
            } else if (beep_type == 1) {
                play_astromech_beep(base_pitch, base_pitch + abs(pitch_bend), duration);
            } else {
                play_astromech_beep(base_pitch, base_pitch - abs(pitch_bend), duration);
            }

            int pause = 20 + (rand() % 130);
            vTaskDelay(pdMS_TO_TICKS(pause));
            phrase_duration += (duration + pause);
        }
    }
}

// Public function to start the audio system
void r2d2_audio_init(void) {
    ESP_LOGI(TAG, "Initializing ES8311 Audio Codec...");
    
    // Initialize the speaker synchronization mutex
    speaker_mutex = xSemaphoreCreateMutex();
    if (speaker_mutex == NULL) {
        ESP_LOGE(TAG, "Failed to create speaker mutex!");
    }

    speaker_dev = bsp_audio_codec_speaker_init();
    if (speaker_dev) {
        esp_codec_dev_sample_info_t fs = {
            .sample_rate = SAMPLE_RATE,
            .channel = 1,
            .bits_per_sample = 16,
        };
        esp_codec_dev_open(speaker_dev, &fs);
        esp_codec_dev_set_out_vol(speaker_dev, 100);
    }
    
    // Pin the voice task to Core 1 so it doesn't interrupt the UI on Core 0
    xTaskCreatePinnedToCore(r2d2_voice_task, "r2d2_voice", 4096, NULL, 5, NULL, 1);
    ESP_LOGI(TAG, "Procedural Voice Synthesizer Online.");
}

// Struct for passing speak text to task
typedef struct {
    char *text;
} speak_task_args_t;

// One-shot FreeRTOS task to translate text to beeps on Core 1
static void speak_text_task(void *pvParameters) {
    speak_task_args_t *args = (speak_task_args_t *)pvParameters;
    char *text = args->text;
    
    ESP_LOGI(TAG, "Custom speech started for text: %s", text);
    
    for (int i = 0; text[i] != '\0'; i++) {
        char c = text[i];
        if (c == ' ' || c == '\t' || c == '\n' || c == '\r') {
            play_gap(150);
            vTaskDelay(pdMS_TO_TICKS(50));
        } else {
            // Handle case-insensitivity
            if (c >= 'A' && c <= 'Z') {
                c = c - 'A' + 'a';
            }
            
            int seed = (int)c;
            // Generate a deterministic start and end frequency from the character's ASCII code
            int start_freq = 400 + (seed * 19) % 1100;
            int end_freq = start_freq + ((seed * 37) % 500 - 250);
            int duration = 90 + (seed * 11) % 180;
            
            play_astromech_beep(start_freq, end_freq, duration);
            
            // Add a short gap between letters to prevent them from blending
            play_gap(30);
            vTaskDelay(pdMS_TO_TICKS(30));
        }
    }
    
    ESP_LOGI(TAG, "Custom speech translation task finished.");
    free(text);
    free(args);
    vTaskDelete(NULL);
}

void r2d2_speak_text(const char *text) {
    if (text == NULL || text[0] == '\0') {
        return;
    }
    
    speak_task_args_t *args = malloc(sizeof(speak_task_args_t));
    if (args == NULL) {
        ESP_LOGE(TAG, "Failed to allocate memory for speak task args!");
        return;
    }
    
    args->text = strdup(text);
    if (args->text == NULL) {
        ESP_LOGE(TAG, "Failed to duplicate text string!");
        free(args);
        return;
    }
    
    BaseType_t ret = xTaskCreatePinnedToCore(speak_text_task, "r2d2_speak_task", 4096, args, 6, NULL, 1);
    if (ret != pdPASS) {
        ESP_LOGE(TAG, "Failed to create FreeRTOS speak task!");
        free(args->text);
        free(args);
    }
}