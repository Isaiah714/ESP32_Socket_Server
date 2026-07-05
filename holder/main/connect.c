#include <string.h>
#include <inttypes.h>
#include <freertos/event_groups.h>

#include "connect.h"

#define TAG "simple_connect"
#define WIFI_AUTHMODE WIFI_AUTH_WPA2_PSK
#define WIFI_CONNECTED_BIT BIT0
#define WIFI_FAIL_BIT BIT1

static const int WIFI_RETRY_ATTEMPT = 3;
static int wifi_retry_count = 0;

static esp_netif_t * wifi_netif = NULL;
static esp_event_handler_instance_t ip_event_handler;
static esp_event_handler_instance_t wifi_event_hanlder;

static EventGroupHandle_t s_wifi_event_group = NULL;

/*
 * nvs_flash_init() initializes non volatile memory (NVS)
 * which stores data even when the computer is shut off
 * 
 * esp_netif_int() initializes the network interface
 * which is the TCP/IP stack
 * 
 * esp_event_loop_create_default() creates a default 
 * event loop to allow components to declare events
 * so that other components can register handlers
 */

// Initializing the hardware and interface needed to set up wifi
esp_err_t init_wifi() {

  esp_err_t esp_nvs = nvs_flash_init();

  if(esp_nvs == ESP_ERR_NVS_NO_FREE_PAGES ||
     esp_nvs == ESP_ERR_NVS_NEW_VERSION_FOUND) {

    ESP_ERROR_CHECK(nvs_flash_erase());
    esp_nvs = nvs_flash_init();

  }

  s_wifi_event_group = xEventGroupCreate();
  
  esp_nvs = esp_netif_init();
  if(esp_nvs != ESP_OK) {
    ESP_LOGE(TAG, "Failed to initialize TCP/IP network stack...");
    return esp_nvs;
  }

  esp_nvs = esp_event_loop_create_default();
  if(esp_nvs != ESP_OK) {
    ESP_LOGE(TAG, "Failed to create default event loop");
    return esp_nvs;
  }

  esp_nvs = esp_wifi_set_default_wifi_sta_handlers();
  if(esp_nvs != ESP_OK) {
    ESP_LOGE(TAG, "Failed to set default handlers");
    return esp_nvs;
  }

  wifi_netif = esp_netif_create_default_wifi_sta();
  if(wifi_netif == NULL) {
    ESP_LOGE(TAG, "Failed to create default Wifi STA interface");
    return ESP_FAIL;
  }

  wifi_init_config_t config = WIFI_INIT_CONFIG_DEFAULT();
  ESP_ERROR_CHECK(esp_wifi_init(&config));

  ESP_ERROR_CHECK(esp_event_handler_instance_register(WIFI_EVENT,
                                                      ESP_EVENT_ANY_ID,
                                                      &wifi_event_cb,
                                                      NULL,
                                                      &wifi_event_handler));
  ESP_ERROR_CHECK(esp_event_handler_instance_register(IP_EVENT,
                                                      ESP_EVENT_ANY_ID,
                                                      &ip_event_cb,
                                                      NULL,
                                                      &ip_event_handler));
}

esp_err_t init_connect() {
  // Initializing some systems before starting wifi connection
  ESP_ERROR_CHECK(nvs_flash_init());
  ESP_ERROR_CHECK(esp_netif_init());
  ESP_ERROR_CHECK(esp_event_loop_create_default());

  

  /*
   * After establishing a connection. We can print 
   * access point information 
   */  
  wifi_ap_record_t access_point_info;
  ESP_ERROR_CHECK(esp_wifi_sta_get_ap_info(&access_point_info));
  ESP_LOGI(TAG, "---Access Point Information---");
  ESP_LOG_BUFFER_HEX("MAC Address", access_point_info.bssid, sizeof(access_point_info.bssid));
  ESP_LOG_BUFFER_CHAR("SSID", access_point_info.ssid, sizeof(access_point_info.ssid));
  ESP_LOGI(TAG, "Primary Channel: %d", access_point_info.primary);
  ESP_LOGI(TAG, "RSSI: (Signal Strength) %d", access_point_info.rssi);


}