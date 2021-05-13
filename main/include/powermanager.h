#include "freertos/FreeRTOS.h"
#include "freertos/task.h"

#include "stdio.h"
#include "string.h"

#include "mwifi.h"

#include "esp_log.h"

#include "ina219.h"

#define I2C_PORT 0
#define I2C_ADDR INA219_ADDR_GND_GND
#if defined(CONFIG_IDF_TARGET_ESP8266)
    #define SDA_GPIO 4
    #define SCL_GPIO 5
#else
    #define SDA_GPIO 21
    #define SCL_GPIO 22
#endif

bool is_running = true;

static const char *TAG = "MESH";

void ina219_task(void *pvParameters) {
    MDF_LOGI("INA219 task is running");

    mdf_err_t ret = MDF_OK;
    uint32_t timestamp;
    char *data = MDF_MALLOC(MWIFI_PAYLOAD_LEN);
    size_t size = MWIFI_PAYLOAD_LEN;
    mwifi_data_type_t data_type = {
        .compression = true,
        .custom = MQTT_SEND,
    };
    cJSON *json_root = NULL;
    cJSON *json_data = NULL;

    time_t now = 0;

    ina219_t dev;
    memset(&dev, 0, sizeof(ina219_t));

    ESP_ERROR_CHECK(ina219_init_desc(&dev, I2C_ADDR, I2C_PORT, SDA_GPIO, SCL_GPIO));
    ESP_LOGD(TAG, "Initializing INA219");
    ESP_ERROR_CHECK(ina219_init(&dev));

    ESP_LOGD(TAG, "Configuring INA219");
    ESP_ERROR_CHECK(ina219_configure(&dev, INA219_BUS_RANGE_16V, INA219_GAIN_0_125,
            INA219_RES_12BIT_1S, INA219_RES_12BIT_1S, INA219_MODE_CONT_SHUNT_BUS));

    ESP_LOGD(TAG, "Calibrating INA219");
    ESP_ERROR_CHECK(ina219_calibrate(&dev, 5.0, 0.1)); // 5A max current, 0.1 Ohm shunt resistance

    float bus_voltage, shunt_voltage, current, power;
    char ina219_buffer[1024];

    ESP_LOGD(TAG, "Starting the INA219 loop");
    while (mwifi_is_connected()) {
        ESP_ERROR_CHECK(ina219_get_bus_voltage(&dev, &bus_voltage));
        ESP_ERROR_CHECK(ina219_get_shunt_voltage(&dev, &shunt_voltage));
        ESP_ERROR_CHECK(ina219_get_current(&dev, &current));
        ESP_ERROR_CHECK(ina219_get_power(&dev, &power));

        // parse data to json
        timestamp = time(&now);
        snprintf(data, size, "{\
            \"measurement\": \"power_manager\",\
            \"tags\": {\
                \"region\": \"sicily\",\
                \"city\": \"messina\"\
            },\
            \"fields\": {\
               \"bus_voltage\": %.04f,\
               \"shunt_voltage\": %.04f,\
               \"current\": %.04f,\
               \"power\": %.04f\
            },\
            \"timestamp\": %d\
        }", bus_voltage, shunt_voltage, current, power, timestamp);
        json_root = cJSON_Parse(data);
        MDF_ERROR_CONTINUE(!json_root, "cJSON_Parse, data format error");
        json_data = cJSON_GetObjectItem(json_root, "data");
        char *send_data = cJSON_PrintUnformatted(json_data);
        MDF_LOGD("Send MQTT_SEND packet to [ROOT] size: %d, data: %s", size, data);

        // send data to root
        ret = mwifi_write(NULL, &data_type, data, size, true);
        MDF_ERROR_GOTO(ret != MDF_OK, FREE_MEM, "<%s> mwifi_root_write", mdf_err_to_name(ret));
        
FREE_MEM:
        MDF_FREE(send_data);
        
        vTaskDelay(1000 / portTICK_PERIOD_MS);


        // snprintf(ina219_buffer, sizeof(ina219_buffer), "{, \"shunt_voltage\": %.04f, \"current\": %.04f, \"power\": %.04f}", bus_voltage, shunt_voltage, current, power); // VBUS (V), VSHUNT (mV), IBUS (mA), PBUS (mW) 
        // printf("%s\n", ina219_buffer);

        // vTaskDelay(1000 / portTICK_PERIOD_MS);
    }

    MDF_LOGW("YL-69 task is exit");
    MDF_FREE(data);
    vTaskDelete(NULL);
}

void powermanager_setup() {
    ESP_ERROR_CHECK(i2cdev_init());
}

void powermanager_start() {
    char* pc_ina219_name = "ina219_task";
    xTaskCreate(ina219_task, pc_ina219_name, configMINIMAL_STACK_SIZE * 8, NULL, 5, NULL);
}