#ifndef R2D2_AUDIO_H
#define R2D2_AUDIO_H

void r2d2_audio_init(void);

// Play one of 3 songs in R2-D2 style (0=Back in Black, 1=Happy Birthday, 2=Never Gonna Give You Up)
void r2d2_play_song(int song_id);

// Global flag to indicate when audio is actively playing a tone
#include <stdbool.h>
extern volatile bool r2d2_is_speaking;

// Speak a custom text phrase by translating it into deterministic beeps
void r2d2_speak_text(const char *text);

#endif // R2D2_AUDIO_H