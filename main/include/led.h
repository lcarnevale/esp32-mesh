/*
 * ESP32 Mesh Network
 * Copyright 2021, FCRLab at University of Messina (Messina, Italy)
 * 
 * @maintainer: Lorenzo Carnevale <lcarnevale@unime.it>
 */


#include "mdf_err.h"
#include "driver/gpio.h"

static const char *TAG = "LED";

#define LED_GPIO CONFIG_LED_GPIO

mdf_err_t led_on(void);
mdf_err_t led_off(void);
static void configure_led(void);

static void configure_led(void) {
    MDF_LOGD("Configuring the GPIO LED");
    gpio_reset_pin(LED_GPIO);
    gpio_set_direction(LED_GPIO, GPIO_MODE_OUTPUT);
}

mdf_err_t led_on(void) {
    gpio_set_level(LED_GPIO, true);
    return MDF_OK;
}

mdf_err_t led_off(void) {
    gpio_set_level(LED_GPIO, false);
    return MDF_OK;
}