/*
 * ESP32 Mesh Network
 * Copyright 2021, FCRLab at University of Messina (Messina, Italy)
 * 
 * @maintainer: Lorenzo Carnevale <lcarnevale@unime.it>
 */

#include "esp_log.h"
#include "mupgrade.h"


#include "mprotocol.h"
#include "mqtt_manager.h"


static const char *TAG = "MESH";


static void root_reader_task(void *arg) {
    mdf_err_t ret = MDF_OK;
    char *data    = MDF_MALLOC(MWIFI_PAYLOAD_LEN);
    size_t size   = MWIFI_PAYLOAD_LEN;
    mwifi_data_type_t data_type      = {0};
    uint8_t src_addr[MWIFI_ADDR_LEN] = {0};

    char *mqtt_buffer = MDF_MALLOC(MWIFI_PAYLOAD_LEN);

    MDF_LOGI("Root reader task is ran");

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
            snprintf(mqtt_buffer, size, "%.*s", strlen(data), data);
            mqtt_publish(mqtt_buffer);
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
