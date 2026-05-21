#ifndef R2D2_MODES_H
#define R2D2_MODES_H

typedef enum {
    MODE_AUDIO_IDLE = 0,
    MODE_DISPLAY
} r2d2_mode_t;

extern volatile r2d2_mode_t current_mode;

void r2d2_toggle_mode(void);
r2d2_mode_t r2d2_get_mode(void);
void r2d2_set_mode(r2d2_mode_t mode);

#endif // R2D2_MODES_H
