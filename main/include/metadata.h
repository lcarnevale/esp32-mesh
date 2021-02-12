/*
 * ESP32 Mesh Network
 * Copyright 2021, FCRLab at University of Messina (Messina, Italy)
 * 
 * @maintainer: Lorenzo Carnevale <lcarnevale@unime.it>
 */

#include "mlink.h"

#define SIZE_MAC_ADDR 18

char sta_mac_addr[SIZE_MAC_ADDR] = {0};

/**
 *  Keep up to date about the used MDF version.
 */ 
const char *MDF_VERSION = mdf_get_version();

/**
 * @brief All module events will be sent to this task in esp-mdf
 *
 * @Note:
 *     1. Do not block or lengthy operations in the callback function.
 *     2. Do not consume a lot of memory in the callback function.
 *        The task memory of the callback function is only 4KB.
 */
mdf_err_t get_sta_mac_address() {
    uint8_t derived_mac_addr[6] = {0};
    ESP_ERROR_CHECK(esp_read_mac(derived_mac_addr, ESP_MAC_WIFI_STA));
    snprintf(sta_mac_addr, 18, "%x:%x:%x:%x:%x:%x",
             derived_mac_addr[0], derived_mac_addr[1], derived_mac_addr[2],
             derived_mac_addr[3], derived_mac_addr[4], derived_mac_addr[5]);
    return MDF_OK;
}