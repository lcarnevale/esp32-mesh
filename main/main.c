/*
 * ESP32 Mesh Network
 * Copyright 2017, Espressif Systems (Shanghai) PTE LTD
 * Copyright 2021, FCRLab at University of Messina (Messina, Italy)
 * 
 * @maintainer: Lorenzo Carnevale <lcarnevale@unime.it>
 */

#include "mwifi.h"
#include "mdf_err.h"

#include "events.h"
#include "metadata.h"


static mdf_err_t wifi_init() {
    mdf_err_t ret          = nvs_flash_init();
    wifi_init_config_t cfg = WIFI_INIT_CONFIG_DEFAULT();

    if (ret == ESP_ERR_NVS_NO_FREE_PAGES || ret == ESP_ERR_NVS_NEW_VERSION_FOUND) {
        MDF_ERROR_ASSERT(nvs_flash_erase());
        ret = nvs_flash_init();
    }

    MDF_ERROR_ASSERT(ret);

    tcpip_adapter_init();
    MDF_ERROR_ASSERT(esp_event_loop_init(NULL, NULL));
    MDF_ERROR_ASSERT(esp_wifi_init(&cfg));
    MDF_ERROR_ASSERT(esp_wifi_set_storage(WIFI_STORAGE_FLASH));
    MDF_ERROR_ASSERT(esp_wifi_set_mode(WIFI_MODE_STA));
    MDF_ERROR_ASSERT(esp_wifi_set_ps(WIFI_PS_NONE));
    MDF_ERROR_ASSERT(esp_mesh_set_6m_rate(false));
    MDF_ERROR_ASSERT(esp_wifi_start());

    return MDF_OK;
}

static mdf_err_t mesh_init() {
    mwifi_init_config_t cfg = MWIFI_INIT_CONFIG_DEFAULT();
    mwifi_config_t config   = {
        .router_ssid     = CONFIG_ROUTER_SSID,
        .router_password = CONFIG_ROUTER_PASSWORD,
        // .channel         = CONFIG_MESH_CHANNEL,
        .mesh_id         = CONFIG_MESH_ID,
        .mesh_password   = CONFIG_MESH_PASSWORD,
        // .mesh_type       = CONFIG_DEVICE_TYPE,
    };

    MDF_ERROR_ASSERT(mdf_event_loop_init(event_mesh_callback));
    MDF_ERROR_ASSERT(mwifi_init(&cfg));
    MDF_ERROR_ASSERT(mwifi_set_config(&config));
    
    return MDF_OK;
}

void setup(void) {
    esp_log_level_set("*", ESP_LOG_DEBUG);
    esp_log_level_set("gpio", ESP_LOG_WARN);
    
    char *sta_mac_addr = get_mac_address(ESP_MAC_WIFI_STA);
    MDF_ERROR_ASSERT(wifi_init());
    MDF_ERROR_ASSERT(mesh_init());
}

void start(void) {
    MDF_ERROR_ASSERT(mwifi_start());
}

void app_main() {
    setup();
    start();
}
