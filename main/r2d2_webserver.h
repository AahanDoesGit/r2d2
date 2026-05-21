#pragma once

#ifdef __cplusplus
extern "C" {
#endif

/**
 * @brief Start the HTTP web server for the R2-D2 dashboard.
 *        Should be called after Wi-Fi has successfully obtained an IP address.
 */
void r2d2_webserver_start(void);

#ifdef __cplusplus
}
#endif
