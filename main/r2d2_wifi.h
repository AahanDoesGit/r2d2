#pragma once

#ifdef __cplusplus
extern "C" {
#endif

/**
 * @brief Initialize NVS, configure Wi-Fi station, and connect to the network.
 *        This function blocks until the IP is acquired or connection fails.
 */
void r2d2_wifi_init(void);

#ifdef __cplusplus
}
#endif
