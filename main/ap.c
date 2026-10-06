#include <stdio.h>
#include <stdbool.h>
#include <string.h>
#include <unistd.h>
#include "esp_err.h"
#include "esp_check.h"
#include "esp_log.h"
#include "esp_wifi.h"
#include "esp_wifi_default.h"
#include "esp_wifi_types_generic.h"

#define AP_SSID                     "KeyboardViking"
#define AP_PASSWORD                 "evelknievel"

static const char *TAG = "soft_ap";

esp_err_t start_soft_ap(void) {
    // Initialize netif
    ESP_ERROR_CHECK(esp_netif_init());

    esp_netif_t *ap_interface = esp_netif_create_default_wifi_ap();
    if (ap_interface == NULL) {
        ESP_LOGE(TAG, "Failed to create AP network interface"); 
        return ESP_FAIL;
    }

    // Initialize wifi
    wifi_init_config_t wificonfig = WIFI_INIT_CONFIG_DEFAULT();
    ESP_RETURN_ON_ERROR(esp_wifi_init(&wificonfig), TAG, "WIFI init failed");
    ESP_RETURN_ON_ERROR(esp_wifi_set_mode(WIFI_MODE_AP), TAG, "Could not set MODE");

    wifi_config_t ap_config = {
        .ap = {
            .ssid = AP_SSID,
            .password = AP_PASSWORD,
            .ssid_len = strlen(AP_SSID),
            .channel = 6,
            .authmode = WIFI_AUTH_WPA2_PSK,
            .max_connection = 1,
            .pmf_cfg = {
                .required = false,
            },
        },
    };

    ESP_RETURN_ON_ERROR(esp_wifi_set_config(WIFI_IF_AP, &ap_config), TAG, "Could not set CONFIG");
    ESP_RETURN_ON_ERROR(esp_wifi_start(), TAG, "Could not start WIFI");

    return ESP_OK;
}
