/*
 * ESP32 MQTT Driver
 * Copyright (c) 2021, Lorenzo Carnevale <lcarnevale@unime.it>
 */

#include <stdio.h>
#include <string.h>

#include "esp_log.h"

#include "mdf_err.h"
#include "mdf_mem.h"
#include "mwifi.h"
#include "mupgrade.h"

#include "jsmn.h"
#include "mqtt_manager.h"


#define BROKER_URL "mqtt://broker.mqttdashboard.com"
#define OTA_TOPIC "/unime/fcrlab/ponmetro/smartagriculture/ota/endpoint"
#define READING_TOPIC "/unime/fcrlab/ponmetro/smartagriculture/reading"
static const char *TAG = "MQTT_MANAGER";

bool is_connected = false;

jsmn_parser parser;
jsmntok_t tokens[10]; /* We expect no more than 10 JSON tokens */

esp_mqtt_client_handle_t client = {0};


esp_mqtt_client_config_t get_mqtt_client_config() {
    esp_mqtt_client_config_t mqtt_cfg = {
        .uri = BROKER_URL,
    };

    return mqtt_cfg;
}


void mqtt_event_handler(void *arg,  esp_event_base_t event_base, int32_t event_id, void *event_data);
void mqtt_connect() {
    ESP_LOGD(TAG, "Connecting to MQTT broker: %s\n", BROKER_URL);

    esp_mqtt_client_config_t mqtt_cfg = get_mqtt_client_config();
    client = esp_mqtt_client_init(&mqtt_cfg);
    esp_mqtt_client_register_event(client, ESP_EVENT_ANY_ID, mqtt_event_handler, client);
    esp_mqtt_client_start(client);
}

void mqtt_disconnect() {
    if (is_connected)
        esp_mqtt_client_disconnect(client);
}

esp_err_t mqtt_event_handler_cb(esp_mqtt_event_handle_t event);
void mqtt_event_handler(void *arg,  esp_event_base_t event_base, int32_t event_id, void *event_data) {
    ESP_LOGD(TAG, "Event dispatched from event loop base=%s, event_id=%d", event_base, event_id);
    mqtt_event_handler_cb(event_data);
}

esp_err_t mqtt_event_connected(esp_mqtt_client_handle_t client);
esp_err_t mqtt_event_disconnected(esp_mqtt_client_handle_t client);
esp_err_t mqtt_event_data(char* topic, int topic_len, char* data, int data_len);
esp_err_t mqtt_event_handler_cb(esp_mqtt_event_handle_t event) {
    switch (event->event_id) {
        case MQTT_EVENT_CONNECTED:
            mqtt_event_connected(event->client);
            break;
        case MQTT_EVENT_DISCONNECTED:
            mqtt_event_disconnected(event->client);
            break;
        case MQTT_EVENT_SUBSCRIBED:
            ESP_LOGI(TAG, "MQTT_EVENT_SUBSCRIBED to topic %.*s", event->topic_len, event->topic);
            break;
        case MQTT_EVENT_UNSUBSCRIBED:
            ESP_LOGW(TAG, "MQTT_EVENT_UNSUBSCRIBED from topic %.*s", event->topic_len, event->topic);
            break;
        case MQTT_EVENT_PUBLISHED:
            ESP_LOGD(TAG, "MQTT_EVENT_PUBLISHED, topic=%s", event->topic);
            break;
        case MQTT_EVENT_DATA:
            // TODO: make it thread safe
            mqtt_event_data(event->topic, event->topic_len, event->data, event->data_len);
            break;
        case MQTT_EVENT_ERROR:
            ESP_LOGE(TAG, "MQTT_EVENT_ERROR");
            break;
        default:
            ESP_LOGW(TAG, "Other event id:%d", event->event_id);
            break;
    }
    return ESP_OK;
}

esp_err_t mqtt_event_connected(esp_mqtt_client_handle_t client) {
    ESP_LOGD(TAG, "MQTT client is connected");
    
    // subscribe default topics
    // TODO: move topics to configuration file
    // TODO: create a loop of topics
    char *topic = OTA_TOPIC;
    esp_mqtt_client_subscribe(client, topic, 0);
    ESP_LOGI(TAG, "subscription to %s ... completed", topic);
    is_connected = true;

    return ESP_OK;
}

esp_err_t mqtt_event_disconnected(esp_mqtt_client_handle_t client) {
    ESP_LOGI(TAG, "MQTT client is disconnected");
    
    // unsubscribe default topics
    // TODO: move topics to configuration file
    // TODO: create a loop of topics
    char *topic = OTA_TOPIC;
    ESP_LOGD(TAG, "unsubscription from %s ... \r", topic);
    esp_mqtt_client_unsubscribe(client, topic);
    is_connected = false;

    return ESP_OK;
}

void parse_topic(char* data, int data_len);
esp_err_t mqtt_event_data(char* topic, int topic_len, char* data, int data_len) {
    ESP_LOGI(TAG, "Received data from topic %.*s", topic_len, topic);
    char topic_compare[200] = {0};
    snprintf(topic_compare, topic_len+1, "%s", topic);
    int r = strcmp(OTA_TOPIC, topic_compare);
    if ( !r )
        parse_topic(data, data_len);
    return ESP_OK;
}

endpoint get_ota_endpoint(char* data, int data_len);
static void ota_task( void * pvParameters );
void parse_topic(char* data, int data_len) {
    endpoint ota_endpoint = get_ota_endpoint(data, data_len);
    // ESP_LOGI(TAG, "OTA run %.*s", strlen(ota_endpoint.url), ota_endpoint.url);
    char *buffer = MDF_MALLOC(MWIFI_PAYLOAD_LEN);
    size_t size = MWIFI_PAYLOAD_LEN;
    snprintf(buffer, size, "%.*s", strlen(ota_endpoint.url), ota_endpoint.url);
    xTaskCreate(ota_task, "ota_task", 4*1024, (void*) buffer, CONFIG_MDF_TASK_DEFAULT_PRIOTY, NULL);
    return;
}

static int jsoneq(const char *json, jsmntok_t *tok, const char *s);
endpoint get_ota_endpoint(char* data, int data_len) {
    endpoint ota_endpoint = {0};
    jsmn_init(&parser);
    int r = jsmn_parse(&parser, data, data_len, tokens, 10);
    for (int i = 1; i < r; i++) {
        if (jsoneq(data, &tokens[i], "ota_endpoint") == 0) {
            ota_endpoint.url_len = tokens[i+1].end - tokens[i+1].start;
            char* str1 = data + tokens[i+1].start;
            // ESP_LOGI(TAG, "OTA Endpoint received %.*s", ota_endpoint.url_len, data + tokens[i+1].start);
            //ota_endpoint.url = (char *) malloc( sizeof(ota_endpoint.url_len) );
            strncpy( ota_endpoint.url, str1, ota_endpoint.url_len );
            ota_endpoint.url[ota_endpoint.url_len] = '\0';
            continue;
        }
    }
    return ota_endpoint;
}

static int jsoneq(const char *json, jsmntok_t *tok, const char *s) {
  if (tok->type == JSMN_STRING && (int)strlen(s) == tok->end - tok->start &&
      strncmp(json + tok->start, s, tok->end - tok->start) == 0) {
    return 0;
  }
  return -1;
}

static void ota_task( void *parameter ) {
    char *ota_endpoint;
    ota_endpoint = (char *) parameter;
    ESP_LOGI(TAG, "OTA run %.*s", strlen(ota_endpoint), ota_endpoint);

    mdf_err_t ret       = MDF_OK;
    uint8_t *data       = MDF_MALLOC(MWIFI_PAYLOAD_LEN);
    char name[32]       = {0x0};
    size_t total_size   = 0;
    // int start_time      = 0;
    mupgrade_result_t upgrade_result = {0};
    mwifi_data_type_t data_type = {.communicate = MWIFI_COMMUNICATE_MULTICAST};

    /**
     * @note If you need to upgrade all devices, pass MWIFI_ADDR_ANY;
     *       If you upgrade the incoming address list to the specified device
     */
    // uint8_t dest_addr[][MWIFI_ADDR_LEN] = {{0x1, 0x1, 0x1, 0x1, 0x1, 0x1}, {0x2, 0x2, 0x2, 0x2, 0x2, 0x2},};
    uint8_t dest_addr[][MWIFI_ADDR_LEN] = {MWIFI_ADDR_ANY};

    // /**
    //  * @brief In order to allow more nodes to join the mesh network for firmware upgrade,
    //  *      in the example we will start the firmware upgrade after 30 seconds.
    //  */
    // vTaskDelay(10 * 1000 / portTICK_PERIOD_MS);

    esp_http_client_config_t config = {
        .url            = ota_endpoint,
        .transport_type = HTTP_TRANSPORT_UNKNOWN,
    };

    /**
     * @brief 1. Connect to the server
     */
    esp_http_client_handle_t client = esp_http_client_init(&config);
    MDF_ERROR_GOTO(!client, EXIT, "Initialise HTTP connection");

    // start_time = xTaskGetTickCount();

    MDF_LOGI("Open HTTP connection: %s", ota_endpoint);

    /**
     * @brief First, the firmware is obtained from the http server and stored on the root node.
     */
    do {
        ret = esp_http_client_open(client, 0);

        if (ret != MDF_OK) {
            if (!esp_mesh_is_root()) {
                goto EXIT;
            }
            vTaskDelay(pdMS_TO_TICKS(1000));
            MDF_LOGW("<%s> Connection service failed", mdf_err_to_name(ret));
        }
    } while (ret != MDF_OK);

    total_size = esp_http_client_fetch_headers(client);
    sscanf(ota_endpoint, "%*[^//]//%*[^/]/%[^.bin]", name);

    if (total_size <= 0) {
        MDF_LOGW("Please check the address of the server");
        ret = esp_http_client_read(client, (char *)data, MWIFI_PAYLOAD_LEN);
        MDF_ERROR_GOTO(ret < 0, EXIT, "<%s> Read data from http stream", mdf_err_to_name(ret));
        MDF_LOGW("Recv data: %.*s", ret, data);
        goto EXIT;
    }

    /**
     * @brief 2. Initialize the upgrade status and erase the upgrade partition.
     */
    ret = mupgrade_firmware_init(name, total_size);
    MDF_ERROR_GOTO(ret != MDF_OK, EXIT, "<%s> Initialize the upgrade status", mdf_err_to_name(ret));

    /**
     * @brief 3. Read firmware from the server and write it to the flash of the root node
     */
    for (ssize_t size = 0, recv_size = 0; recv_size < total_size; recv_size += size) {
        size = esp_http_client_read(client, (char *)data, MWIFI_PAYLOAD_LEN);
        MDF_ERROR_GOTO(size < 0, EXIT, "<%s> Read data from http stream", mdf_err_to_name(ret));

        if (size > 0) {
            /* @brief  Write firmware to flash */
            ret = mupgrade_firmware_download(data, size);
            MDF_ERROR_GOTO(ret != MDF_OK, EXIT, "<%s> Write firmware to flash, size: %d, data: %.*s",
                           mdf_err_to_name(ret), size, size, data);
        } else {
            MDF_LOGW("<%s> esp_http_client_read", mdf_err_to_name(ret));
            goto EXIT;
        }
    }

    // MDF_LOGI("The service download firmware is complete, Spend time: %ds", (xTaskGetTickCount() - start_time) * portTICK_RATE_MS / 1000);

    /**
     * @brief 4. The firmware will be sent to each node.
     */
    // start_time = xTaskGetTickCount();
    ret = mupgrade_firmware_send((uint8_t *)dest_addr, sizeof(dest_addr) / MWIFI_ADDR_LEN, &upgrade_result);
    MDF_ERROR_GOTO(ret != MDF_OK, EXIT, "<%s> mupgrade_firmware_send", mdf_err_to_name(ret));

    if (upgrade_result.successed_num == 0) {
        MDF_LOGW("Devices upgrade failed, unfinished_num: %d", upgrade_result.unfinished_num);
        goto EXIT;
    }

    // MDF_LOGI("Firmware is sent to the device to complete, Spend time: %ds", (xTaskGetTickCount() - start_time) * portTICK_RATE_MS / 1000);
    // MDF_LOGI("Devices upgrade completed, successed_num: %d, unfinished_num: %d", upgrade_result.successed_num, upgrade_result.unfinished_num);

    /**
     * @brief 5. the root notifies nodes to restart
     */
    const char *restart_str = "restart";
    ret = mwifi_root_write(upgrade_result.successed_addr, upgrade_result.successed_num, &data_type, restart_str, strlen(restart_str), true);
    MDF_ERROR_GOTO(ret != MDF_OK, EXIT, "<%s> mwifi_root_recv", mdf_err_to_name(ret));

EXIT:
    MDF_FREE(data);
    mupgrade_result_free(&upgrade_result);
    esp_http_client_close(client);
    esp_http_client_cleanup(client);
    vTaskDelete(NULL);
}

void mqtt_publish_task(void* parameters);
void mqtt_publish(char *data) {
    ESP_LOGD(TAG, "Publishing MQTT message ...");
    char* pc_name = "mqtt_publish_task";
    xTaskCreate(mqtt_publish_task, pc_name, 4*1024, (void*) data, CONFIG_MDF_TASK_DEFAULT_PRIOTY, NULL);
}

void mqtt_publish_task(void *parameters) {
    char *data;
    data = (char *) parameters;

    int len = strlen(data);
    int qos = 1;
    int retain = 1;
    char *topic = READING_TOPIC;
    esp_mqtt_client_publish(client, topic, data, len, qos, retain);

    vTaskDelete(NULL);
}


int topic_is(char* topic, char* topic_target) {
    int r = strcmp(topic, topic_target);
    ESP_LOGI(TAG, "comparison is %d", r);
    return r;
}
