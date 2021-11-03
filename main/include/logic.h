/*
 * ESP32 Mesh Network
 * Copyright 2021, FCRLab at University of Messina (Messina, Italy)
 * 
 * @maintainer: Lorenzo Carnevale <lcarnevale@unime.it>
 */

#include "esp_log.h"
#include "mupgrade.h"

#include "led.h"
#include "protocols.h"
// #include "mqtt_manager.h"
// #include "powermanager.h"

// static const char *TAG = "LOGIC";

void run_setup_led_task(void);
static void __setup_led_task(void);
void run_led_on_task(void);
static void __led_on_task(void);
void run_led_off_task(void);
static void __led_off_task(void);
void run_node_read_task(void);
void run_led_blink_task(void);
static void __led_blink_task(void);
static void node_read_task(void *arg);
void run_node_write_task(void);
static void node_write_task(void *arg);

void run_root_reader_task(void);
static void root_reader_task(void *arg);
void run_node_executer_tasks(void);



void run_setup_led_task(void) {
    char* pcName = "setup_task";
    xTaskCreate(
        __setup_led_task, 
        pcName, 
        4*1024, 
        NULL, 
        CONFIG_MDF_TASK_DEFAULT_PRIOTY, 
        NULL
    );
}

static void __setup_led_task (void) {
    configure_led();
    vTaskDelete(NULL);
}

void run_led_on_task(void) {
    char* pcName = "run_led_on_task";
    xTaskCreate(
        __led_on_task, 
        pcName, 
        4*1024, 
        NULL, 
        CONFIG_MDF_TASK_DEFAULT_PRIOTY, 
        NULL
    );
}

static void __led_on_task(void) {
    led_on();
    vTaskDelete(NULL);
}

void run_led_off_task(void) {
    char* pcName = "run_led_on_task";
    xTaskCreate(
        __led_off_task, 
        pcName, 
        4*1024, 
        NULL, 
        CONFIG_MDF_TASK_DEFAULT_PRIOTY, 
        NULL
    );
}

static void __led_off_task(void) {
    led_off();
    vTaskDelete(NULL);
}

void run_led_blink_task(void) {
    char* pcName = "run_led_blink_task";
    xTaskCreate(
        __led_blink_task, 
        pcName, 
        4*1024, 
        NULL, 
        CONFIG_MDF_TASK_DEFAULT_PRIOTY, 
        NULL
    );
}

static void __led_blink_task(void) {
    for (;;) {
        led_on();
        vTaskDelay(1000 / portTICK_RATE_MS);
        led_off();
        vTaskDelay(1000 / portTICK_RATE_MS);
    }
    vTaskDelete(NULL);
}


void run_node_read_task(void) {
    char* pcName = "node_read_task";
    xTaskCreate(
        node_read_task,
        pcName,
        4*1024,
        NULL,
        CONFIG_MDF_TASK_DEFAULT_PRIOTY,
        NULL
    );
}

static void node_read_task(void *arg) {
    mdf_err_t ret                    = MDF_OK;
    char *data                       = MDF_MALLOC(MWIFI_PAYLOAD_LEN);
    size_t size                      = MWIFI_PAYLOAD_LEN;
    mwifi_data_type_t data_type      = {0x0};
    uint8_t src_addr[MWIFI_ADDR_LEN] = {0x0};

    MDF_LOGI("Note read task is running");

    for (;;) {
        if (!mwifi_is_connected()) {
            vTaskDelay(500 / portTICK_RATE_MS);
            continue;
        }

        size = MWIFI_PAYLOAD_LEN;
        memset(data, 0, MWIFI_PAYLOAD_LEN);
        ret = mwifi_read(src_addr, &data_type, data, &size, portMAX_DELAY);
        MDF_ERROR_CONTINUE(ret != MDF_OK, "<%s> mwifi_read", mdf_err_to_name(ret));
        
        // if (data_type.upgrade) { // This mesh package contains upgrade data.
        //     ret = mupgrade_handle(src_addr, data, size);
        //     MDF_ERROR_CONTINUE(ret != MDF_OK, "<%s> mupgrade_handle", mdf_err_to_name(ret));
        // } else {
        MDF_LOGI("Node receive: " MACSTR ", size: %d, data: %s", MAC2STR(src_addr), size, data);

        //     /**
        //      * @brief Finally, the node receives a restart notification. Restart it yourself..
        //      */
        //     if (!strcmp(data, "restart")) {
        //         MDF_LOGI("Restart the version of the switching device");
        //         MDF_LOGW("The device will restart after 3 seconds");
        //         vTaskDelay(pdMS_TO_TICKS(3000));
        //         esp_restart();
        //     }
        // }
    }

    MDF_LOGW("Note read task is exit");

    MDF_FREE(data);
    vTaskDelete(NULL);
}

void run_node_write_task(void) {
    char* pcName = "node_write_task";
    xTaskCreate(
        node_write_task,
        pcName,
        4*1024,
        NULL,
        CONFIG_MDF_TASK_DEFAULT_PRIOTY,
        NULL
    );
}

static void node_write_task(void *arg) {
    size_t size                     = 0;
    int count                       = 0;
    char *data                      = NULL;
    mdf_err_t ret                   = MDF_OK;
    mwifi_data_type_t data_type     = {0};
    uint8_t sta_mac[MWIFI_ADDR_LEN] = {0};

    MDF_LOGI("NODE task is running");

    esp_wifi_get_mac(ESP_IF_WIFI_STA, sta_mac);

    for (;;) {
        if (!mwifi_is_connected()) {
            vTaskDelay(500 / portTICK_RATE_MS);
            continue;
        }

        size = asprintf(&data, "{\"src_addr\": \"" MACSTR "\",\"count\": %d}",
                        MAC2STR(sta_mac), count++);

        MDF_LOGI("Node send, size: %d, data: %s", size, data);
        ret = mwifi_write(NULL, &data_type, data, size, true);
        MDF_FREE(data);
        MDF_ERROR_CONTINUE(ret != MDF_OK, "<%s> mwifi_write", mdf_err_to_name(ret));

        vTaskDelay(3000 / portTICK_RATE_MS);
    }

    MDF_FREE(data);
    MDF_LOGW("NODE task is exit");

    vTaskDelete(NULL);
}







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
            // mqtt_publish(mqtt_buffer);
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

void run_node_executer_tasks(void) {
    // Running INA219 task
    // powermanager_setup();
    // powermanager_start();
}

