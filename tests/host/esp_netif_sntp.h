#pragma once

#include <stdbool.h>
#include <stddef.h>

#include "esp_err.h"
#include "freertos/FreeRTOS.h"

/* ESP-IDF's default; ntp_sync_two_servers_test passes 2 as sdkconfig.defaults sets it. */
#ifndef CONFIG_LWIP_SNTP_MAX_SERVERS
#define CONFIG_LWIP_SNTP_MAX_SERVERS 1
#endif

typedef struct {
    bool smooth_sync;
    bool server_from_dhcp;
    bool wait_for_sync;
    bool start;
    void *sync_cb;
    bool renew_servers_after_new_IP;
    int ip_event_to_renew;
    size_t index_of_first_server;
    size_t num_of_servers;
    const char *servers[CONFIG_LWIP_SNTP_MAX_SERVERS];
} esp_sntp_config_t;

#define ESP_SNTP_SERVER_LIST(...) { __VA_ARGS__ }
#define ESP_NETIF_SNTP_DEFAULT_CONFIG_MULTIPLE(servers_in_list, list_of_servers) { \
    .wait_for_sync = true,                                                          \
    .start = true,                                                                  \
    .num_of_servers = (servers_in_list),                                            \
    .servers = list_of_servers,                                                     \
}
#define ESP_NETIF_SNTP_DEFAULT_CONFIG(server) \
    ESP_NETIF_SNTP_DEFAULT_CONFIG_MULTIPLE(1, {server})

esp_err_t esp_netif_sntp_init(const esp_sntp_config_t *config);
void esp_netif_sntp_deinit(void);
esp_err_t esp_netif_sntp_sync_wait(TickType_t tout);
