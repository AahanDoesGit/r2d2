# R2-D2 ESP32-P4 Astromech System

A complete hardware and firmware ecosystem for building a smart R2-D2 droid, powered by the ESP32-P4 with a 4.3" touch display, audio synthesis, and a full-stack web dashboard.

## Overview

This project turns an ESP32-P4 board into the brain of an astromech droid. It features:
*   **Dual-Core Architecture:** UI and display logic run on Core 0, while procedural audio generation (beeps and boops) runs on Core 1 to ensure zero jitter.
*   **Dynamic LVGL 9 Display:** Features a blinking R2-D2 hologram interface that reacts to speech, transitions to a Star Wars-themed data matrix (quotes & time), and plays GIFs from the SD card.
*   **Procedural Audio Synth:** Real-time beep and chirp generation. Type text into the dashboard, and the droid will "speak" it through uniquely generated audio tones.
*   **Web Dashboard:** A completely embedded Star Wars-styled web app served directly from the ESP32, allowing remote control, text-to-speech, and GIF uploads.
*   **Hardware Control:** Physical buttons to trigger songs or switch display modes.

---

## Hardware Requirements

*   **ESP32-P4 Development Board:** Specifically, the Waveshare ESP32-P4-WIFI6-Touch-LCD-4.3 (v1.3).
*   **Micro-SD Card:** Formatted to FAT32. This stores user-uploaded GIFs and the droid's face JPEG.
*   **2Ω Speaker:** Connects to the ES8311 audio codec on the board.
*   **Momentary Push-buttons (x2):** Connected to GPIO 48 (trigger song) and GPIO 47 (toggle mode).

---

## Software Prerequisites

1.  **ESP-IDF v5.5:** Ensure you have the ESP-IDF set up on your machine.
2.  **LVGL 9:** Managed automatically via ESP-IDF component manager (`idf_component.yml`).

---

## Project Structure

```text
r2d2-system/
├── CMakeLists.txt           # Build configuration
├── main/
│   ├── main.c               # Boot orchestrator
│   ├── r2d2_audio.c/h       # Procedural audio synth & songs (Core 1)
│   ├── r2d2_display.c/h     # LVGL 9 UI, JPEG & GIF decoding (Core 0)
│   ├── r2d2_webserver.c/h   # HTTP server & embedded HTML dashboard
│   ├── r2d2_wifi.c/h        # Wi-Fi STA and SNTP time sync
│   ├── r2d2_buttons.c/h     # GPIO polling logic
│   └── r2d2_modes.c/h       # State machine management
├── managed_components/      # LVGL and AnimatedGIF library
└── sd_card/                 # Assets
    └── images/R2-D2.jpg     # Embedded R2-D2 face image
```

---

## Build and Flash Instructions

1.  **Clone the repository:**
    ```bash
    git clone https://github.com/AahanDoesGit/r2d2.git
    cd r2d2-system
    ```

2.  **Load the ESP-IDF environment** (assuming standard installation path):
    ```bash
    source ~/esp/esp-idf/export.sh
    ```

3.  **Build the firmware:**
    ```bash
    idf.py build
    ```

4.  **Flash the firmware and open the serial monitor:**
    ```bash
    # Replace /dev/cu.usbmodem5B5E0698681 with your board's specific port
    idf.py -p /dev/cu.usbmodem5B5E0698681 flash monitor
    ```

---

## Usage Guide

### The Web Dashboard
Once the droid boots up, it will connect to your configured Wi-Fi network and print its IP address to the serial monitor.
1.  Open a web browser and enter the IP address.
2.  **Translate Dialogue:** Type a message into the console and hit "TRANSMIT TO DROID". The droid will blink its red eye and generate procedural audio tones based on your text.
3.  **Toggle Screen:** Switch the display between the standard R2-D2 face and the "Dialogue Translation Matrix" (which shows a clock, scrolling quotes, and starfield).
4.  **Hologram Projection (GIFs):** Upload standard GIF files directly through the dashboard. Click "PLAY" on a thumbnail, and the droid will display the animated GIF on its screen for 20 seconds.

### Physical Controls
*   **Button 1 (GPIO 48):** Plays a random Star Wars song/theme from the internal library.
*   **Button 2 (GPIO 47):** Toggles the screen mode between the static face and the dynamic data matrix.

---

## Customization

### Changing the Face Image
To change the R2-D2 face image, replace the `sd_card/images/R2-D2.jpg` file with a new 480x800 JPEG image. The firmware embeds this file at build time, so simply run `idf.py build` again to bundle the new image into the firmware.

### Adjusting the "Eye"
The blinking red dot's size and coordinates are defined in `main/r2d2_display.c` inside the `r2d2_display_init()` function. You can tweak the `lv_obj_align()` offsets if your custom JPEG has the eye in a different location.
