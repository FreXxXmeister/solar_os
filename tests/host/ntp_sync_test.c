#include <assert.h>
#include <stdio.h>
#include <string.h>

#include "esp_netif_sntp.h"
#include "esp_timer.h"
#include "nvs.h"
#include "solar_os_time.h"

size_t strlcpy(char *dst, const char *src, size_t size)
{
    const size_t len = strlen(src);
    if (size > 0) {
        const size_t copy = len < size - 1 ? len : size - 1;
        memcpy(dst, src, copy);
        dst[copy] = '\0';
    }
    return len;
}

int64_t esp_timer_get_time(void) { return 0; }
esp_err_t nvs_open(const char *name, nvs_open_mode_t mode, nvs_handle_t *handle)
{
    (void)name;
    (void)mode;
    (void)handle;
    return ESP_ERR_NVS_NOT_FOUND;
}
esp_err_t nvs_commit(nvs_handle_t handle) { (void)handle; return ESP_OK; }
void nvs_close(nvs_handle_t handle) { (void)handle; }
esp_err_t nvs_get_str(nvs_handle_t handle, const char *key, char *value, size_t *length)
{
    (void)handle;
    (void)key;
    (void)value;
    (void)length;
    return ESP_ERR_NVS_NOT_FOUND;
}
esp_err_t nvs_set_str(nvs_handle_t handle, const char *key, const char *value)
{
    (void)handle;
    (void)key;
    (void)value;
    return ESP_OK;
}

static int deinit_calls;
static esp_err_t init_result = ESP_OK;
static bool nest_in_wait;
static esp_err_t nested_result;
static int nested_deinit_calls;
static esp_sntp_config_t last_config;
/* lwIP's server slots: esp_netif_sntp_init sets only the first num_of_servers of them,
 * and neither esp_netif_sntp_deinit nor sntp_stop clears the rest. */
static const char *lwip_slots[CONFIG_LWIP_SNTP_MAX_SERVERS];

esp_err_t esp_netif_sntp_init(const esp_sntp_config_t *config)
{
    assert(config != NULL && config->num_of_servers >= 1);
    assert(config->num_of_servers <= CONFIG_LWIP_SNTP_MAX_SERVERS);
    last_config = *config;
    for (size_t i = 0; i < config->num_of_servers; i++) {
        lwip_slots[i] = config->servers[i];
    }
    return init_result;
}

void esp_netif_sntp_deinit(void) { deinit_calls++; }

esp_err_t esp_netif_sntp_sync_wait(TickType_t tout)
{
    (void)tout;
    if (nest_in_wait) {
        /* A second caller while the first one waits, as the ntp-sync job and a shell ntp. */
        nest_in_wait = false;
        const int before = deinit_calls;
        nested_result = solar_os_time_ntp_sync("pool.ntp.org", 0, NULL, NULL);
        nested_deinit_calls = deinit_calls - before;
    }
    return ESP_ERR_TIMEOUT;
}

static void test_second_sync_is_refused_without_touching_sntp(void)
{
    nest_in_wait = true;
    deinit_calls = 0;
    assert(solar_os_time_ntp_sync("pool.ntp.org", 0, NULL, NULL) == ESP_ERR_TIMEOUT);
    assert(nested_result == ESP_ERR_INVALID_STATE);
    assert(nested_deinit_calls == 0);
    assert(deinit_calls == 2);
}

static void test_sync_runs_again_after_each_way_out(void)
{
    init_result = ESP_ERR_NO_MEM;
    assert(solar_os_time_ntp_sync("pool.ntp.org", 0, NULL, NULL) == ESP_ERR_NO_MEM);
    init_result = ESP_OK;
    assert(solar_os_time_ntp_sync("bad name", 0, NULL, NULL) == ESP_ERR_INVALID_ARG);
    assert(solar_os_time_ntp_sync("pool.ntp.org", 0, NULL, NULL) == ESP_ERR_TIMEOUT);
}

static void test_pool_gets_esphome_servers(void)
{
    /* ESPHome's default servers, one per slot (esphome/components/sntp/time.py). */
    static const char *const esphome[] = {"0.pool.ntp.org", "1.pool.ntp.org", "2.pool.ntp.org"};
    static const char *const servers[] = {NULL, "pool.ntp.org"}; /* the default, and named */
    assert(CONFIG_LWIP_SNTP_MAX_SERVERS <= 3);
    for (size_t s = 0; s < 2; s++) {
        assert(solar_os_time_ntp_sync(servers[s], 0, NULL, NULL) == ESP_ERR_TIMEOUT);
        assert(last_config.num_of_servers == CONFIG_LWIP_SNTP_MAX_SERVERS);
        for (size_t i = 0; i < CONFIG_LWIP_SNTP_MAX_SERVERS; i++) {
            assert(strcmp(last_config.servers[i], esphome[i]) == 0);
        }
    }
}

static void test_named_server_is_asked_alone(void)
{
    /* After a default sync, as lwIP can stay on a later slot when a pool server did not answer. */
    assert(solar_os_time_ntp_sync(NULL, 0, NULL, NULL) == ESP_ERR_TIMEOUT);
    assert(solar_os_time_ntp_sync("time.cloudflare.com", 0, NULL, NULL) == ESP_ERR_TIMEOUT);
    for (size_t i = 0; i < CONFIG_LWIP_SNTP_MAX_SERVERS; i++) {
        assert(strcmp(lwip_slots[i], "time.cloudflare.com") == 0);
    }
}

int main(void)
{
    test_second_sync_is_refused_without_touching_sntp();
    test_sync_runs_again_after_each_way_out();
    test_pool_gets_esphome_servers();
    test_named_server_is_asked_alone();
    puts("ntp sync tests passed");
    return 0;
}
