#include "esp_err.h"
#include "esp_event.h"

#include "freertos/event_groups.h"

#include "mqtt_client.h"


typedef struct {
    char url[100];      /* URL in string format */
    int url_len;    /* Length of the URL for this endpoint */
} endpoint;


void mqtt_connect();
void mqtt_disconnect();
void mqtt_publish(char *data);