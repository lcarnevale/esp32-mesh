/*
 * ESP32 Mesh Network
 * Copyright 2021, FCRLab at University of Messina (Messina, Italy)
 * 
 * @maintainer: Lorenzo Carnevale <lcarnevale@unime.it>
 */

#include "mlink.h"

#define SIZE_MAC_ADDR 18


/**
 * Keep up to date about the used MDF version.
 * 
 * @return The current MDF version.
 */ 
const char *MDF_VERSION = mdf_get_version();

/**
 * Read MAC Address according to device type.
 * 
 * @param esp_mac_type_t Enum defined by esp-idf for selecting between 
 *  station, access point, bluetooth and ethernet
 * @return The mac address as array of chars
 */
char* get_mac_address(esp_mac_type_t mac_type) {
    char mac_addr[SIZE_MAC_ADDR]    = {0};
    uint8_t derived_mac_addr[6]     = {0};
    switch (mac_type) {
        case ESP_MAC_WIFI_STA:
            ESP_ERROR_CHECK(esp_read_mac(derived_mac_addr, ESP_MAC_WIFI_STA));
            break;
        default:
            break;
    }
    snprintf(mac_addr, 18, "%x:%x:%x:%x:%x:%x",
        derived_mac_addr[0], derived_mac_addr[1], derived_mac_addr[2],
        derived_mac_addr[3], derived_mac_addr[4], derived_mac_addr[5]);
    return mac_addr;
}