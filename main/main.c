/*
 * ESP32 Mesh Network
 * Copyright 2017, Espressif Systems (Shanghai) PTE LTD
 * Copyright 2021, FCRLab at University of Messina (Messina, Italy)
 * 
 * @maintainer: Lorenzo Carnevale <lcarnevale@unime.it>
 */

#include "mwifi.h"
#include "mupgrade.h"
#include "mprotocol.h"
#include "msntp.h"
#include "esp_log.h"
#include "mqtt_manager.h"
#include "metadata.h"


static const char *TAG = "MESH";
static bool is_connected = false;


static void root_reader_task(void *arg) {
    mdf_err_t ret = MDF_OK;
    char *data    = MDF_MALLOC(MWIFI_PAYLOAD_LEN);
    size_t size   = MWIFI_PAYLOAD_LEN;
    mwifi_data_type_t data_type      = {0};
    uint8_t src_addr[MWIFI_ADDR_LEN] = {0};

    char *mqtt_buffer = MDF_MALLOC(MWIFI_PAYLOAD_LEN);

    MDF_LOGD("Root reader task is ran");

    while (mwifi_is_connected()) {
        size = MWIFI_PAYLOAD_LEN;
        memset(data, 0, MWIFI_PAYLOAD_LEN);
        ret = mwifi_root_read(src_addr, &data_type, data, &size, portMAX_DELAY);
        MDF_ERROR_CONTINUE(ret != MDF_OK, "<%s> mwifi_root_recv", mdf_err_to_name(ret));

        if (data_type.upgrade) { // this mesh package contains upgrade data.
            ret = mupgrade_root_handle(src_addr, data, size);
            MDF_ERROR_CONTINUE(ret != MDF_OK, "<%s>, mupgrade_root_handle", mdf_err_to_name(ret));
        } else if (data_type.custom == MQTT_SEND) {
            MDF_LOGD("Receive MQTT_SEND packet from [NODE] addr: " MACSTR ", size: %d, data: %s", MAC2STR(src_addr), size, data);
            if ( is_connected ) {
                snprintf(mqtt_buffer, size, "%.*s", strlen(data), data);
                mqtt_publish(mqtt_buffer);
            }
        } else {
            MDF_LOGW("Receive UNKNOWN packet from [NODE] addr: " MACSTR ", size: %d, data: %s", MAC2STR(src_addr), size, data);
        }
    }

    MDF_LOGW("Root reader task is ended");

    MDF_FREE(data);
    vTaskDelete(NULL);
}

/**
 * @brief Handling data between wifi mesh devices.
 */
static void node_reader_task(void *arg) {
    mdf_err_t ret = MDF_OK;
    char *data    = MDF_MALLOC(MWIFI_PAYLOAD_LEN);
    size_t size   = MWIFI_PAYLOAD_LEN;
    mwifi_data_type_t data_type      = {0x0};
    uint8_t src_addr[MWIFI_ADDR_LEN] = {0};

    MDF_LOGI("Node read task is running");

    while (mwifi_is_connected()) {
        size = MWIFI_PAYLOAD_LEN;
        memset(data, 0, MWIFI_PAYLOAD_LEN);
        ret = mwifi_read(src_addr, &data_type, data, &size, portMAX_DELAY);
        MDF_ERROR_CONTINUE(ret != MDF_OK, "<%s> mwifi_root_recv", mdf_err_to_name(ret));

        if (data_type.upgrade) { // This mesh package contains upgrade data.
            ret = mupgrade_handle(src_addr, data, size);
            MDF_ERROR_CONTINUE(ret != MDF_OK, "<%s> mupgrade_handle", mdf_err_to_name(ret));
        } else {
            MDF_LOGI("Receive [ROOT] addr: " MACSTR ", size: %d, data: %s", MAC2STR(src_addr), size, data);

            /**
             * @brief Finally, the node receives a restart notification. Restart it yourself..
             */
            if (!strcmp(data, "restart")) {
                MDF_LOGI("Restart the version of the switching device");
                MDF_LOGW("The device will restart after 3 seconds");
                vTaskDelay(pdMS_TO_TICKS(3000));
                esp_restart();
            }
        }
    }

    MDF_LOGW("Node read task is exit");

    MDF_FREE(data);
    vTaskDelete(NULL);
}

bool node_is_root(void) {
    return esp_mesh_get_layer() == MESH_ROOT_LAYER;
}

void run_node_reader_task(void) {
    MDF_LOGD("Running node read task ...");
    char* pcName = "node_reader_task";
    xTaskCreate(node_reader_task, pcName, 4*1024, NULL, CONFIG_MDF_TASK_DEFAULT_PRIOTY, NULL);
}

void run_root_reader_task(void) {
    MDF_LOGD("Running root task ...");
    char* pcName = "root_reader_task";
    xTaskCreate(root_reader_task, pcName, 4*1024, NULL, CONFIG_MDF_TASK_DEFAULT_PRIOTY, NULL);
}


/**
 * @brief All module events will be sent to this task in esp-mdf
 *
 * @Note:
 *     1. Do not block or lengthy operations in the callback function.
 *     2. Do not consume a lot of memory in the callback function.
 *        The task memory of the callback function is only 4KB.
 */
static mdf_err_t event_loop_cb(mdf_event_loop_t event, void *ctx) {
    switch (event) {
        case MDF_EVENT_MWIFI_PARENT_CONNECTED:
            run_node_reader_task();
            if ( node_is_root() )
                run_root_reader_task();
            break;
        case MDF_EVENT_MWIFI_PARENT_DISCONNECTED:
            if ( node_is_root() )
                mqtt_disconnect();
            break;
        case MDF_EVENT_MWIFI_ROOT_GOT_IP:
            MDF_LOGI("Got IP address");
            if ( node_is_root() ) {
                setup_sntp();
                sync_sntp();
                mqtt_connect();
                is_connected = true;
            }
            break;
        case MDF_EVENT_MWIFI_ROOT_LOST_IP:
            MDF_LOGW("Lost IP address");
            if ( node_is_root() )
                is_connected = false;
            break;
        case MDF_EVENT_MUPGRADE_STARTED: {
            mupgrade_status_t status = {0x0};
            mupgrade_get_status(&status);
            MDF_LOGI("MDF_EVENT_MUPGRADE_STARTED, name: %s, size: %d", status.name, status.total_size);
            break;
        }
        case MDF_EVENT_MUPGRADE_STATUS:
            MDF_LOGI("Upgrade progress: %d%%", (int)ctx);
            break;
        default:
            break;
    }

    return MDF_OK;
}


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
        .mesh_id         = CONFIG_MESH_ID,
    };

    MDF_ERROR_ASSERT(mdf_event_loop_init(event_loop_cb));
    MDF_ERROR_ASSERT(mwifi_init(&cfg));
    MDF_ERROR_ASSERT(mwifi_set_config(&config));

    return MDF_OK;
}

void setup(void) {
    esp_log_level_set("*", ESP_LOG_INFO);
    esp_log_level_set("mupgrade_node", ESP_LOG_DEBUG);
    esp_log_level_set("mupgrade_root", ESP_LOG_DEBUG);
    esp_log_level_set(TAG, ESP_LOG_DEBUG);
    
    MDF_ERROR_ASSERT(wifi_init());
    MDF_ERROR_ASSERT(mesh_init());
    MDF_ERROR_ASSERT(get_sta_mac_address());
}

void start(void) {
    MDF_ERROR_ASSERT(mwifi_start());
}

void app_main() {
    setup();
    start();
}
