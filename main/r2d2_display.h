#ifndef R2D2_DISPLAY_H
#define R2D2_DISPLAY_H

#include <stddef.h>

void r2d2_display_init(void);
void r2d2_display_set_quote(const char *quote);
void r2d2_display_get_quote(char *buf, size_t max_len);
void r2d2_display_refresh_quote(void);
void r2d2_display_play_gif(const char *filename);

#endif // R2D2_DISPLAY_H