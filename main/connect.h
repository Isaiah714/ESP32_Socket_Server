#ifndef __CONNECT__
#define __CONNECT__

#include <esp_err.h>

// I will probably never use doxygen but who knows (Following tutorial)
/**
 * @brief Event group bits for WiFi events
 */
#define WIFI_STA_CONNECTED_BIT     BIT0
#define WIFI_STA_IPV4_OBTAINED_BIT BIT1
#define WIFI_STA_IPV6_OBTAINED_BIT BIT2

esp_err_t init_wifi();
esp_err_t wifi_connect(char * wifi_ssid, char * wifi_password);
esp_err_t wifi_disconnect();
esp_err_t deinit_wifi();

/**
 * @brief Initialize WiFi in station (STA) mode.
 * 
 * Set up the WiFi interface and connect a to a WiFi network. You can see the
 * event group to wait for a connection and IP address
 * 
 * Important! You must call esp_netif_init() and esp_event_loop_create_default()
 * before calling this function.
 * 
 * @param[in] event_group Event group handle for WiFi and IP events. Pass NULL
 *                        to use the existing event group which is for restarting
 *                        the driver if you already passed an event group once 
 *                        since this function will retain a static pointer to an
 *                        event group
 * @return 
 *  - ESP_OK on success
 *  - Other errors on failure. See esp_err.h for error codes.
 */
esp_err_t wifi_sta_init(EventGroupHandle_t event_group);

/**
 * @brief Disable WiFi
 * 
 * @return
 *  - ESP_OK on success
 *  - Other errors on failure. See esp_err.h for error codes.
 */
esp_err_t wifi_sta_stop();

/**
 * @brief Attempt to reconnect WiFi
 * 
 * @return
 *  - ESP_OK on success
 *  - Other errors on failure. See esp_err.h for error codes.
 */
esp_err_t wifi_sta_reconnect();

#endif
