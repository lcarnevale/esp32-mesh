/*
 * ESP32 Simple Network Time Protocol Driver
 * Copyright 2021, FCRLab at University of Messina (Messina, Italy)
 * 
 * @maintainer: Lorenzo Carnevale <lcarnevale@unime.it>
 */

#include "esp_sntp.h"

#define SNTP_ENDPOINT "pool.ntp.org"

// static const char *TAG = "SNTP_MANAGER";

void setup_sntp(void) {
    sntp_setoperatingmode(SNTP_OPMODE_POLL);
    sntp_setservername(0, SNTP_ENDPOINT);
    sntp_init();
}

void sync_sntp(void) {
    int retry = 0;
    const int retry_count = 10;
    while (sntp_get_sync_status() == SNTP_SYNC_STATUS_RESET && ++retry < retry_count) {
        // ESP_LOGD(TAG, "Waiting for system time to be set... (%d/%d)", retry, retry_count);
        vTaskDelay(2000 / portTICK_PERIOD_MS);
    }
}