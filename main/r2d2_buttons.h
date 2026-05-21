#ifndef R2D2_BUTTONS_H
#define R2D2_BUTTONS_H

// GPIO pins for the two physical buttons (wired with pull-up, active-LOW)
#define BUTTON_1_GPIO  48
#define BUTTON_2_GPIO  47

/**
 * @brief Initialize both buttons with internal pull-up resistors.
 *        Spawns a FreeRTOS task that polls for presses and logs them.
 *        Replace the log calls with your own actions (sound, display change, etc.)
 */
void r2d2_buttons_init(void);

#endif // R2D2_BUTTONS_H
