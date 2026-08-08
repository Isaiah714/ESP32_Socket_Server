#include <esp_event.h>
#include <esp_log.h>
#include <esp_private/wifi.h>
#include <esp_wifi.h>
#include <esp_wifi_netif.h>

#include "connect.h"

/**
 * This will let know where we are 
 * in our WiFi driver stack
 */
static const char * TAG = "wifi_sta";

/**
 * Declaring global variables, first is pointer 
 * to the WiFi network interface, second is the
 * event group, and the third is handle to the 
 * driver
 */
static esp_netif_t * s_wifi_netif = NULL;
static EventGroupHandle_t s_wifi_event_group = NULL;
static wifi_netif_driver_t s_wifi_driver = NULL;

// When an event occurs, it triggers an event with a callback
static void on_wifi_event(void * arg,
                          esp_event_base_t event_base,
                          int32_t event_id,
                          void * event_data);
/**
 * DHCP (e.g when recieving or losing an IP address from the router)
 * DHCP stands for Dynamic Host Configuration Protocol which 
 * automatically assigns IP addresses and other communication parameters
 * to devices connected to the network.
 */
static void on_ip_event(void * arg,
                        esp_event_base_t event_base,
                        int32_t event_id,
                        void * event_data);

// Internal wifi start to call various driver elements
static void wifi_start(void * esp_netif,
                       esp_event_base_t base,
                       int32_t event_id,
                       void * data);

//*********************FUNCTION DEFINITIONS*********************//
static void on_wifi_event(void * arg,
                          esp_event_base_t event_base,
                          int32_t event_id,
                          void * event_data)
{
  switch(event_id) {
    case WIFI_EVENT_STA_START:
    if(s_wifi_netif != NULL) {
      wifi_start(s_wifi_netif, event_base, event_id, event_data);
    }
    break;

    case WIFI_EVENT_STA_STOP:
    if(s_wifi_netif != NULL) {
      esp_netif_action_stop(s_wifi_netif, event_base, event_id, event_data);
    }
    break;

    case WIFI_EVENT_STA_CONNECTED:
    if(s_wifi_netif == NULL) {
      ESP_LOGE(TAG, "WiFi not started: interface handle is NULL");
      return;  
    }

    // Print AP information
    wifi_event_sta_connected_t * event_sta_connected = (wifi_event_sta_connected_t *)event_data;
    ESP_LOGI(TAG, "Connected to AP");
    ESP_LOGI(TAG, "   SSID: %s", (char*)event_sta_connected->ssid);
    ESP_LOGI(TAG, "   Channel: %d", event_sta_connected->channel);
    ESP_LOGI(TAG, "   Auth mode: %d", event_sta_connected->authmode);
    ESP_LOGI(TAG, "   AID: %d", event_sta_connected->aid);

    // (s2.4) Register interface recieve callback (from esp-idf wifi article)
    /**
     * We will get the driver from our network interface that is a handle to 
     * backend driver
     */
    wifi_netif_driver_t driver = esp_netif_get_io_driver(s_wifi_netif);
    if(!esp_wifi_is_if_ready_when_started(driver)) {
      esp_err_t esp_ret = esp_wifi_register_if_rxcb(driver,
                                                    esp_netif_receive,
                                                    s_wifi_netif);
      if(esp_ret != ESP_OK) {
        ESP_LOGE(TAG, "Failed to register WiFi RX callback");
        return;
      }
    }
    // (s4.2) Set up the WiFi interface and start DHCP process
    /**
     * This sets up the full interace and starts the DHCP process
     * where we will get an IP address from the router 
     */
    esp_netif_action_connected(s_wifi_netif,
                               event_base,
                               event_id,
                               event_data);
    // Set WiFi connected bit
    xEventGroupSetBits(s_wifi_event_group, WIFI_STA_CONNECTED_BIT);
/**
 * The ESP32 handles IPv6 addresses differently, we have manually obtained
 * address if we enable IPv6. We need to call esp_netif_create_ip6_linklocal()
 * to generate a request to obtain a IPv6 address. To be able to enable to get
 * IPv6 address, you need to modify the Kconfigbuild file to do so.
 */
#if CONFIG_WIFI_STA_CONNECT_IPV6 || CONFIG_WIFI_STA_CONNECT_UNSPECIFIED
      // Request IPv6 link-local address for the interface
      esp_ret = esp_netif_create_ip6_linklocal(s_wifi_netif);
      if(esp_ret != ESP_OK) {
        ESP_LOGE(TAG, "Failed to create IPv6 link-local address");
      }
#endif

      break;
      // Handle disconnect WiFi event
      case WIFI_EVENT_STA_DISCONNECTED:
      if(s_wifi_netif != NULL) {
        esp_netif_action_disconnected(s_wifi_netif,
                                      event_base,
                                      event_id,
                                      event_data);
      }
      xEventGroupClearBits(s_wifi_event_group, WIFI_STA_CONNECTED_BIT);

// If we add this field in the Kconfigbuild file, we can re-attempt to connect
#if CONFIG_WIFI_STA_AUTO_RECONNECT
      ESP_LOGI(TAG, "Attempting to reconnect...");
      wifi_sta_reconnect();
#endif
      break;
  }
}

// Event handler: recieved IP address on WiFi interface from DHCP
static void on_ip_event(void * arg,
                        esp_event_base_t event_base,
                        int32_t event_id,
                        void * event_data)
{
  esp_err_t esp_ret;

  switch(event_id) {

// For Kconfigbuild file
#if CONFIG_WIFI_STA_CONNECT_IPV4 || CONFIG_WIFI_STA_CONNECT_UNSPECIFIED
    case IP_EVENT_STA_GOT_IP:
    if(s_wifi_netif == NULL) {
      ESP_LOGE(TAG, "On obtain IPv4 addr: Interface handle is NULL");
      return;
    }
    // Set IPv4 obtained bit
    xEventGroupSetBits(s_wifi_event_group, WIFI_STA_IPV4_OBTAINED_BIT);

    /**
     * Print IP address, the IP2STR is converting the data
     * int a string
     * 50:16 left off here
     */
    ip_event_got_ip_t * event_ip = (ip_event_got_ip_t *)event_data;
    esp_netif_ip_info_t * ip_info = &event_ip->ip_info;
    ESP_LOGI(TAG, "WiFi IPv4 address obtained");
    ESP_LOGI(TAG, "   IP address: ", IPSTR, IP2STR(&ip_info->ip));
    ESP_LOGI(TAG, "   Netmask: ", IPSTR, IP2STR(&ip_info->netmask));
    ESP_LOGI(TAG, "   Gateway: ", IPSTR, IP2STR(&ip_info->gw));

    break;

#endif

#if CONFIG_WIFI_STA_CONNECT_IPV6 || CONFIG_WIFI_STA_CONNECT_UNSPECIFIED
    // (s5.2) Got IPv6 address
    case IP_EVENT_GOT_IP6:
    // Make sure we have a valid interface handle
    if(s_wifi_netif == NULL) {
      ESP_LOGE(TAG, "On obtain IPv6 addr: Interface handle is NULL");
      return;
    }
    // Notify the WiFi driver that we got the an IP address
    esp_ret = esp_wifi_internal_set_sta_ip();
    if(esp_ret != ESP_OK) {
      ESP_LOGE(TAG, "Failed to notify WiFi driver of IP address");
    }
    // Set connected bit
    xEventGroupSetBits(s_wifi_event_group, WIFI_STA_IPV6_OBTAINED_BIT);

    /**
     * Print IPv6 address. When working with IPv6 addresses, we 
     * need to manually convert the event_data into IPv6 or else
     * it will implcitly be IPv4. 
     */
    ip_event_got_ip6_t * event_ipv6 = (ip_event_got_ip6_t *)event_data;
    esp_netif_ip6_info_t * ip6_info = &event_ipv6->ip6_info;
    ESP_LOGI(TAG, "WiFi IPv6 address obtained");
    ESP_LOGI(TAG, "IP address: " IPV6STR, IPV6STR(ip6_info->ip));
    ESP_LOGI(TAG, "Netmask: " IPV6STR, IPV6STR(ip6_info->netmask));
    ESP_LOGI(TAG, "Gateway: " IPV6STR, IPV6STR(ip6_info->gw));

    break;

#endif
    // Lost IP address
    case IP_EVENT_STA_LOST_IP:
    xEventGroupClearBits(s_wifi_event_group, WIFI_STA_IPV4_OBTAINED_BIT);
    xEventGroupClearBits(s_wifi_event_group, WIFI_STA_IPV6_OBTAINED_BIT);
    ESP_LOGI(TAG, "WiFi lost IP address");
    break;

    // Default case: do nothing
    default:
    ESP_LOGI(TAG, "Unhandled IP event: %li", event_id);
    break;
  }
}

// Set up the WiFi interface and start DHCP process (called from on_wifi_event)
static void wifi_start(void * esp_netif,
                       esp_event_base_t base,
                       int32_t event_id,
                       void * data)
{
  uint8_t mac_addr[6] = {0};
  esp_err_t esp_ret;

  // (s1.3) Get esp-netif driver handle
  wifi_netif_driver_t driver = esp_netif_get_io_driver(esp_netif);
  if(driver == NULL) {
    ESP_LOGE(TAG, "Failed to get WiFi driver handle");
    return;
  }

  // (s1.3) Get MAC address of WiFi inteface
  esp_ret = esp_wifi_get_if_mac(driver, mac_addr);
  if(esp_ret != ESP_OK) {
    ESP_LOGE(TAG, "Failed to get MAC address");
    return;
  }

  // Print MAC address
  ESP_LOGI(TAG,
           "WiFi MAC address: %02x:%02x:%02x:%02x:%02x:%02x",
           mac_addr[0],
           mac_addr[1],
           mac_addr[2],
           mac_addr[3],
           mac_addr[4],
           mac_addr[5]);
  /**
   * (s1.3) Register netstack buffer reference and free callback
   * The two argumetns we're passing are functions that allocate 
   * buffers and deallocates buffers to the backend driver when
   * necessary.
   */
  esp_ret = esp_wifi_internal_reg_netstack_buf_cb(esp_netif_netstack_buf_ref,
                                                  esp_netif_netstack_buf_free);

  if(esp_ret != ESP_OK) {
    ESP_LOGE(TAG,
             "Error (%d): Netstack callback registration failed",
              esp_ret);
    return;
  }

   // (s1.3) Set MAC address of the WiFi interface
   esp_ret = esp_netif_set_mac(esp_netif, mac_addr);
   if(esp_ret != ESP_OK) {
    ESP_LOGE(TAG, "Failed to set MAC address");
    return;
   }

   // (1.3) Start the WiFi interface
   esp_netif_action_start(esp_netif, base, event_id, data);

   // (s3.3) Connect to WiFi
   ESP_LOGI(TAG, "Connecting to%s...", CONFIG_WIFI_STA_SSID);
   esp_ret = esp_wifi_connect();
   if(esp_ret != ESP_OK) {
    ESP_LOGE(TAG, "Failed to connect to WiFi");
   } 
}

esp_err_t wifi_sta_init(EventGroupHandle_t event_group) {
  esp_err_t esp_ret;

  ESP_LOGI(TAG, "Starting WiFi in station mode...");

  // Save the event group handle
  if(event_group != NULL) {
    s_wifi_event_group = event_group;
  }
  if(s_wifi_event_group == NULL) {
    ESP_LOGE(TAG, "Event group handle is NULL");
    return ESP_FAIL;
  }

  //(s1.3) Create default WiFi network interface
  esp_netif_config_t netif_cfg = ESP_NETIF_DEFAULT_WIFI_STA();
  s_wifi_netif = esp_netif_new(&netif_cfg);
  if(s_wifi_netif == NULL) {
    ESP_LOGE(TAG, "Failed to create WiFi network interface");
    return ESP_FAIL;
  }

  // (s1.3) Create WiFi driver
  s_wifi_driver = esp_wifi_create_if_driver(WIFI_IF_STA);
  if(s_wifi_driver == NULL) {
    ESP_LOGE(TAG, "Failed to create WiFi driver handle");
    return ESP_FAIL;
  }

  // (s1.3) Connect WiFi driver to network interface
  esp_ret = esp_netif_attach(s_wifi_netif, s_wifi_driver);
  if(esp_ret != ESP_OK) {
    ESP_LOGE(TAG, "Failed to attach WiFi driver to network interface");
    return ESP_FAIL;
  }

  esp_ret = esp_event_handler_register(WIFI_EVENT,
                                       WIFI_EVENT_STA_START,
                                       &on_wifi_event,
                                       NULL);
  if(esp_ret != ESP_OK) {
    ESP_LOGE(TAG, "Failed to register WiFi STA start event handler");
    return ESP_FAIL;
  }

  // (s1.3) Register WiFi event: station start
  esp_ret = esp_event_handler_register(WIFI_EVENT,
                                       WIFI_EVENT_STA_START,
                                       &on_wifi_event,
                                       NULL);
  if(esp_ret != ESP_OK) {
    ESP_LOGE(TAG, "Failed to register WiFi STA start event handler");
    return ESP_FAIL;
  }

  // (s1.3) Register WiFi event: station stop
  esp_ret = esp_event_handler_register(WIFI_EVENT,
                                       WIFI_EVENT_STA_STOP,
                                       &on_wifi_event, 
                                       NULL);
  if(esp_ret != ESP_OK) {
    ESP_LOGE(TAG, "Failed to register WiFi STA stop event handler");
    return ESP_FAIL;
  }

  // (s1.3) Register WiFi event: station disconnected
  esp_ret = esp_event_handler_register(WIFI_EVENT,
                                       WIFI_EVENT_STA_DISCONNECTED,
                                       &on_wifi_event, 
                                       NULL);
  if(esp_ret != ESP_OK) {
    ESP_LOGE(TAG, "Failed to register WiFi STA stop event handler");
    return ESP_FAIL;
  }

#if CONFIG_WIFI_STA_CONNECT_IPV4 || CONFIG_WIFI_STA_CONNECT_UNSPECIFIED
  // (s1.3) Register IP event: station got IPv4 address
  esp_ret = esp_event_handler_register(IP_EVENT,
                                       IP_EVENT_STA_GOT_IP,
                                       &on_ip_event,
                                       NULL);
  if(esp_ret != ESP_OK) {
    ESP_LOGE(TAG, "Failed to register IPv4 event handler");
    return ESP_FAIL;
  }

#endif

#if CONFIG_WIFI_STA_CONNECT_IPV6 || CONFIG_WIFI_STA_CONNECT_UNSPECIFIED
  // (s1.3) Register IP event: station got IPv4 address
  esp_ret = esp_event_handler_register(IP_EVENT,
                                       IP_EVENT_STA_GOT_IP6,
                                       &on_ip_event,
                                       NULL);
  if(esp_ret != ESP_OK) {
    ESP_LOGE(TAG, "Failed to register IPv6 event handler");
    return ESP_FAIL;
  }

#endif

// (s1.3) Register IP event: station lost IP address
esp_ret = esp_event_handler_register(IP_EVENT,
                                     IP_EVENT_STA_LOST_IP,
                                     &on_ip_event,
                                     NULL);
  if(esp_ret != ESP_OK) {
    ESP_LOGE(TAG, "Failed to register lost IP address event handler");
    return ESP_FAIL;
  }

  esp_ret = esp_register_shutdown_handler((shutdown_handler_t)esp_wifi_stop);
  if(esp_ret != ESP_OK) {
    ESP_LOGE(TAG, "Failed to register shutdown handler");
    return ESP_FAIL;
  }

  // (s1.4) Initialize WiFi
  wifi_init_config_t cfg = WIFI_INIT_CONFIG_DEFAULT();
  esp_ret = esp_wifi_init(&cfg);
  if(esp_ret != ESP_OK) {
    ESP_LOGE(TAG, "Failed to initialize WiFi");
    return ESP_FAIL;
  }

  // (s2) Set WiFi mode to station (device)
  esp_ret = esp_wifi_set_mode(WIFI_MODE_STA);
  if(esp_ret != ESP_OK) {
    ESP_LOGE(TAG, "Failed to set WiFi mode to station");
    return ESP_FAIL;
  }

  // Configure minimum WiFi authentation mode
#if CONFIG_WIFI_STA_AUTH_OPEN
  const wifi_auth_mode_t auth_mode = WIFI_AUTH_OPEN;
#elif CONFIG_WIFI_STA_AUTH_WEP
  const wifi_auth_mode_t auth_mode = WIFI_AUTH_WEP;
#elif CONFIG_WIFI_STA_AUTH_PSK
  const wifi_auth_mode_t auth_mode = WIFI_AUTH_WPA_PSK;
#elif CONFIG_WIFI_STA_AUTH_WPA2_PSK
  const wifi_auth_mode_t auth_mode = WIFI_AUTH_WPA2_PSK;
#elif CONFIG_WIFI_STA_AUTH_WPA_WPA2_PSK
  const wifi_auth_mode_t auth_mode = WIFI_AUTH_WPA_WPA2_PSK;
#elif CONFIG_WIFI_STA_AUTH_WPA3_PSK
  const wifi_auth_mode_t auth_mode = WIFI_AUTH_WPA3_PSK;
#elif CONFIG_WIFI_STA_AUTH_WPA2_WPA3_PSK
  const wifi_auth_mode_t auth_mode = WIFI_AUTH_WPA2_WPA3_PSK;
#elif CONFIG_WIFI_STA_AUTH_WAPI_PSK
  const wifi_auth_mode_t auth_mode = WIFI_AUTH_WAPI_PSK;
#else
  const wifi_auth_mode_t auth_mode = WIFI_AUTH_OPEN;
#endif

#if CONFIG_WIFI_STA_WPA3_SAE_PWE_HUNT_AND_PECK
  const wifi_saw_pwe_method_t sae_pwe_method = WPA3_SAE_PWE_HUNT_AND_PECK;
#elif CONFIG_WIFI_STA_WPA3_SAE_PWE_H2E
  const wifi_saw_pwe_method_t sae_pwe_method = WPA3_SAE_PWE_HASH_TO_ELEMENT;
#elif CONFIG_WIFI_STA_WPA3_SAE_PWE_BOTH
  const wifi_sae_pwe_method_t sae_pwe_method = WPA3_SAE_PWE_UNSPECIFIED;
#endif

  /**
   * (s2) Configure WiFi connection
   * Note: You can save these settings in NVS for future use if you're 
   * e.g using a provisioning app to set up the device or fast reconnect
   */
  wifi_config_t wifi_config = {
    .sta = {
      .ssid = CONFIG_WIFI_STA_SSID,
      .password = CONFIG_WIFI_STA_PASSWORD,
      .threshold.authmode = auth_mode,
      .sae_pwe_h2e = sae_pwe_method,
      .sae_h2e_identifier = CONFIG_WIFI_STA_WPA3_PASSWORD_ID,
    },
  };

  esp_ret = esp_wifi_set_config(WIFI_IF_STA, &wifi_config);
  if(esp_ret != ESP_OK) {
    ESP_LOGE(TAG, "Failed to set WiFi configuration");
    return ESP_FAIL;
  }

  // (s3.1) Start the WiFi driver
  esp_ret = esp_wifi_start();
  if(esp_ret != ESP_OK) {
    ESP_LOGE(TAG, "Failed to start WiFi driver");
    return ESP_FAIL;
  }

  return ESP_OK;

}

/** 
 * Stop WiFi...This is a simple way of shutting all events
 * down. This is pretty inefficient, but doing it this way
 * is to understand what is going on. 
 */
esp_err_t wifi_sta_stop(void) {
  esp_err_t esp_ret;

  // Print message
  ESP_LOGI(TAG, "Stopping WiFi...");

  // Unregister event: station start
  esp_ret = esp_event_handler_unregister(WIFI_EVENT,
                                         WIFI_EVENT_STA_START,
                                         &on_wifi_event);
  if(esp_ret != ESP_OK) {
    ESP_LOGE(TAG, "Failed to unregister WiFi STA start event handler");
    return ESP_FAIL;
  }

  // Unregister event: station stop
  esp_ret = esp_event_handler_unregister(WIFI_EVENT,
                                         WIFI_EVENT_STA_STOP,
                                         &on_wifi_event);
  if(esp_ret != ESP_OK) {
    ESP_LOGE(TAG, "Failed to unregister WiFi STA stop event handler");
    return ESP_FAIL;
  }

  // Unregister event: station connected
  esp_ret = esp_event_handler_unregister(WIFI_EVENT,
                                         WIFI_EVENT_STA_CONNECTED,
                                         &on_wifi_event);
  if(esp_ret != ESP_OK) {
    ESP_LOGE(TAG, "Failed to unregister WiFi STA connected event handler");
    return ESP_FAIL;
  }

  // Unregister event: station disconnected
  esp_ret = esp_event_handler_unregister(WIFI_EVENT,
                                         WIFI_EVENT_STA_DISCONNECTED,
                                         &on_wifi_event);
  if(esp_ret != ESP_OK) {
    ESP_LOGE(TAG, "Failed to unregister WiFi STA disconnecte  d event handler");
    return ESP_FAIL;
  }

#if CONFIG_WIFI_STA_CONNECT_IPV4 || CONFIG_WIFI_STA_CONNECT_UNSPECIFIED
  // Unregister event: station got IPv4 address
  esp_ret = esp_event_handler_unregister(IP_EVENT, 
                                         IP_EVENT_STA_GOT_IP,
                                         &on_ip_event);
  if(esp_ret != ESP_OK) {
    ESP_LOGE(TAG, "Failed to register IPv4 event handler");
    return ESP_FAIL;
  }
#endif

#if CONFIG_WIFI_STA_CONNECT_IPV6 || CONFIG_WIFI_STA_CONNECT_UNSPECIFIED
  // Unregister event: station got IPv6 address
  esp_ret = esp_event_handler_unregister(IP_EVENT, 
                                         IP_EVENT_STA_GOT_IP6,
                                         &on_ip_event);
  if(esp_ret != ESP_OK) {
    ESP_LOGE(TAG, "Failed to register IPv6 event handler");
    return ESP_FAIL;
  }
#endif

  // Unregister event: station lost ip address
  esp_ret = esp_event_handler_unregister(IP_EVENT, 
                                         IP_EVENT_STA_LOST_IP,
                                         &on_ip_event);
  if(esp_ret != ESP_OK) {
    ESP_LOGE(TAG, "Failed to unregister IP lost event handler");
    return ESP_FAIL;
  }
  // Unregister shutdown handler
  esp_ret = esp_unregister_shutdown_handler(
    (shutdown_handler_t)esp_wifi_stop);
  if(esp_ret == ESP_ERR_INVALID_STATE) {
    ESP_LOGI(TAG, "Shutdown handler already unregistered");
  }
  else if(esp_ret != ESP_OK) {
    ESP_LOGE(TAG, "Failed to unregister to shutdown handler");
    return ESP_FAIL;
  }

  // (s8.1) Disconnect from WiFi
  esp_ret = esp_wifi_disconnect();
  if((esp_ret == ESP_ERR_WIFI_NOT_INIT) ||
     (esp_ret == ESP_ERR_WIFI_NOT_STARTED)) {
      ESP_LOGI(TAG, "WiFi already disconnected");
  }
  else if(esp_ret != ESP_OK) {
    ESP_LOGE(TAG, "Error (%d): Failed to disconnect from WiFi", esp_ret);
    return ESP_FAIL;
  }
  
  // (s8.2) Stop the WiFi driver
  esp_ret = esp_wifi_stop();
  if(esp_ret == ESP_ERR_WIFI_NOT_INIT) {
    ESP_LOGI(TAG, "WiFI driver already stopped");
  }
  else if(esp_ret != ESP_OK) {
    ESP_LOGE(TAG, "Error (%d): Failed to stop WiFi driver", esp_ret);
    return ESP_FAIL;
  }

  // (s8.3) Unload the WiFi driver, free resources
  esp_ret = esp_wifi_deinit();
  if(esp_ret == ESP_ERR_WIFI_NOT_INIT) {
    ESP_LOGI(TAG, "WiFi driver already deinitialized");
  }
  else if(esp_ret != ESP_OK) {
    ESP_LOGE(TAG, "Error (%d): Failed to deinitialize WiFi", esp_ret);
    return ESP_FAIL;
  }

  // Detach and fee the WiFi driver
  if(s_wifi_driver != NULL) {
    esp_wifi_destroy_if_driver(s_wifi_driver);
    s_wifi_driver = NULL;
  }

  // Destroy network interface
  if(s_wifi_netif != NULL) {
    esp_netif_destroy(s_wifi_netif);
  }

  // Clear events group bits
  if(s_wifi_event_group != NULL) {
    xEventGroupClearBits(s_wifi_event_group, WIFI_STA_CONNECTED_BIT);
    xEventGroupClearBits(s_wifi_event_group, WIFI_STA_IPV4_OBTAINED_BIT);
    xEventGroupClearBits(s_wifi_event_group, WIFI_STA_IPV6_OBTAINED_BIT);
  }

  // Set interface handle to NULL
  s_wifi_netif = NULL;

  // Print message
  ESP_LOGI(TAG, "WiFi stopped");

  return ESP_OK;
}

// Attempt reconnection to WiFi
esp_err_t wifi_sta_reconnect() {
  esp_err_t esp_ret;

  // Stop WiFi
  esp_ret = wifi_sta_stop();
  if(esp_ret != ESP_OK) {
    ESP_LOGE(TAG, "Failed to stop WiFi during reconnect");
    return esp_ret;
  }

  // Start WiFi
  esp_ret = wifi_sta_init(NULL);
  if(esp_ret != ESP_OK) {
    ESP_LOGE(TAG, "Failed to initialize WiFi during reconnect");
    return esp_ret;
  }

  return ESP_OK;
}
//**************************************************************//