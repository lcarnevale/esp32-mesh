/*
 * ESP32 Mesh Network
 * Copyright 2021, FCRLab at University of Messina (Messina, Italy)
 * 
 * @maintainer: Lorenzo Carnevale <lcarnevale@unime.it>
 */

#include "mdf_err.h"
#include "mdf_event_loop.h"

#include "logic.h"
#include "msntp.h"


static bool is_connected = false;

mdf_err_t __event_mesh_mwifi_started(void);
mdf_err_t __event_mesh_mwifi_stopped(void);
mdf_err_t __event_mesh_parent_connected(void);
mdf_err_t __event_mesh_parent_disconnected(void);
mdf_err_t __event_mesh_child_connected(void);
mdf_err_t __event_mesh_child_diconnected(void);
mdf_err_t __mdf_event_mwifi_routing_change(void);
mdf_err_t __event_mesh_root_got_ip(void);
mdf_err_t __event_mesh_root_lost_ip(void);
mdf_err_t __event_mesh_mupgrade_started(void);
mdf_err_t event_mesh_callback(mdf_event_loop_t event, void *ctx);


/**
 * @brief All module events will be sent to this task in esp-mdf
 *
 * @Note:
 *     1. Do not block or lengthy operations in the callback function.
 *     2. Do not consume a lot of memory in the callback function.
 *        The task memory of the callback function is only 4KB.
 */
mdf_err_t event_mesh_callback(mdf_event_loop_t event, void *ctx) {
    run_setup_led_task();
    switch (event) {
        case MDF_EVENT_MWIFI_STARTED:
            __event_mesh_mwifi_started();
            break;
        case MDF_EVENT_MWIFI_STOPPED:
            __event_mesh_mwifi_stopped();
            break;
        case MDF_EVENT_MWIFI_PARENT_CONNECTED:
            __event_mesh_parent_connected();
            break;
        case MDF_EVENT_MWIFI_PARENT_DISCONNECTED:
            __event_mesh_parent_disconnected();
            break;
        case MDF_EVENT_MWIFI_CHILD_CONNECTED:
            __event_mesh_child_connected();
            break;
        case MDF_EVENT_MWIFI_CHILD_DISCONNECTED:
            __event_mesh_child_diconnected();
            break;
        case MDF_EVENT_MWIFI_ROUTING_TABLE_ADD:
        case MDF_EVENT_MWIFI_ROUTING_TABLE_REMOVE:
            __mdf_event_mwifi_routing_change();
            break;
        case MDF_EVENT_MWIFI_ROOT_GOT_IP:
            __event_mesh_root_got_ip();
            break;
        case MDF_EVENT_MWIFI_ROOT_LOST_IP:
            __event_mesh_root_lost_ip();
            break;
        case MDF_EVENT_MUPGRADE_STARTED: 
            __event_mesh_mupgrade_started();
            break;
        // case MDF_EVENT_MUPGRADE_STATUS:
        //     MDF_LOGI("Upgrade progress: %d%%", (int)ctx);
        //     break;
        default:
            break;
    }
    return MDF_OK;
}

mdf_err_t __event_mesh_mwifi_started(void) {
    MDF_LOGD("MDF_EVENT_MWIFI_STARTED");
    run_led_on_task();
    run_node_read_task();
    run_node_write_task();
    return MDF_OK;
}

mdf_err_t __event_mesh_mwifi_stopped(void) {
    MDF_LOGW("MDF_EVENT_MWIFI_STOPPED");
    run_led_off_task();
    return MDF_OK;
}

mdf_err_t __event_mesh_parent_connected(void) {
    if (esp_mesh_is_root()) {
        MDF_LOGI("Node is root!");
        run_led_blink_task();
    }
    // run_node_reader_task();
    // if ( node_is_root() )
    //     run_root_reader_task();
    // run_node_executer_tasks();
    return MDF_OK;
}

mdf_err_t __event_mesh_parent_disconnected(void) {
    if (!esp_mesh_is_root()) {
        MDF_LOGI("Node is not root!");
        run_led_on_task();
    }
    // if ( node_is_root() )
        // mqtt_disconnect();
    return MDF_OK;
}

mdf_err_t __event_mesh_child_connected(void) {
    MDF_LOGI("MDF_EVENT_MWIFI_CHILD_CONNECTED");
    return MDF_OK;
}

mdf_err_t __event_mesh_child_diconnected(void) {
    MDF_LOGI("MDF_EVENT_MWIFI_CHILD_DISCONNECTED");
    return MDF_OK;
}

mdf_err_t __mdf_event_mwifi_routing_change(void) {
    MDF_LOGI("total_num: %d", esp_mesh_get_total_node_num());
    return MDF_OK;
}

mdf_err_t __event_mesh_root_got_ip(void) {
    // MDF_LOGI("Root obtains the IP address. It is posted by LwIP stack automatically");
    // if ( node_is_root() ) {
    //     setup_sntp();
    //     sync_sntp();
    //     // mqtt_connect();
    //     run_node_executer_tasks(); // no way, lancia solo se root
    //     is_connected = true;
    // }
    return MDF_OK;
}

mdf_err_t __event_mesh_root_lost_ip(void) {
    // MDF_LOGW("Lost IP address");
    // if ( node_is_root() )
    //     is_connected = false;
    return MDF_OK;
}

mdf_err_t __event_mesh_mupgrade_started(void) {
    mupgrade_status_t status = {0x0};
    mupgrade_get_status(&status);
    MDF_LOGI("MDF_EVENT_MUPGRADE_STARTED, name: %s, size: %d", status.name, status.total_size);
    return MDF_OK;
}
