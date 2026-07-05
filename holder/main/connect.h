#ifndef __CONNECT__
#define __CONNECT__

#include <esp_log.h>   // console   (cmake file)
#include <nvs_flash.h> // nvs_flash (cmake file)
#include <esp_netif.h>
#include <esp_err.h>
#include <esp_event.h>
#include <esp_wifi.h>
#include <freertos/FreeRTOS.h>

esp_err_t init_wifi();
esp_err_t wifi_connect(char * wifi_ssid, char * wifi_password);
esp_err_t wifi_disconnect();
esp_err_t deinit_wifi();

#endif