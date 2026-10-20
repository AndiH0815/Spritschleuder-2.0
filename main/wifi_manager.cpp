#include "wifi_manager.h"
#include "esp_event.h"
#include "esp_log.h"
#include "esp_netif.h"
#include "esp_wifi.h"
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "lwip/inet.h"
#include "nvs.h"
#include <cstring>
WifiManager &WifiManager::getInstance() {
    static WifiManager i;
    return i;
}
esp_err_t WifiManager::init() {
    nvs_handle_t n;
    if (nvs_open("wifi", NVS_READONLY, &n) == ESP_OK) {
        char s[33] = {}, p[65] = {};
        size_t sl = sizeof(s), pl = sizeof(p);
        configured = nvs_get_str(n, "ssid", s, &sl) == ESP_OK &&
                     nvs_get_str(n, "password", p, &pl) == ESP_OK && sl > 1;
        nvs_close(n);
        if (configured) {
            ssid = s;
            password = p;
        }
    }
    ESP_ERROR_CHECK(esp_netif_init());
    ESP_ERROR_CHECK(esp_event_loop_create_default());
    // No AP interface, no AP mode, no hotspot fallback.
    esp_netif_create_default_wifi_sta();
    wifi_init_config_t cfg = WIFI_INIT_CONFIG_DEFAULT();
    ESP_ERROR_CHECK(esp_wifi_init(&cfg));
    ESP_ERROR_CHECK(esp_wifi_set_storage(WIFI_STORAGE_RAM));
    ESP_ERROR_CHECK(esp_event_handler_register(WIFI_EVENT, ESP_EVENT_ANY_ID, eventHandler, this));
    ESP_ERROR_CHECK(esp_event_handler_register(IP_EVENT, IP_EVENT_STA_GOT_IP, eventHandler, this));
    ESP_ERROR_CHECK(esp_wifi_set_mode(WIFI_MODE_STA));
    if (!configured) {
        ESP_LOGW("wifi", "Keine WLAN-Daten: USB-Provisionierung erforderlich, kein Hotspot");
        return ESP_OK;
    }
    wifi_config_t sta = {};
    memcpy(sta.sta.ssid, ssid.data(), ssid.size());
    memcpy(sta.sta.password, password.data(), password.size());
    ESP_ERROR_CHECK(esp_wifi_set_config(WIFI_IF_STA, &sta));
    ESP_ERROR_CHECK(esp_wifi_start());
    xTaskCreate(retryTask, "wifi_retry", 2048, this, 3, nullptr);
    return ESP_OK;
}
void WifiManager::retryTask(void *arg) {
    auto self = static_cast<WifiManager *>(arg);
    for (;;) {
        if (!self->connected)
            esp_wifi_connect();
        vTaskDelay(pdMS_TO_TICKS(5000));
    }
}
void WifiManager::eventHandler(void *arg, esp_event_base_t base, int32_t id, void *) {
    auto self = static_cast<WifiManager *>(arg);
    if (base == WIFI_EVENT && id == WIFI_EVENT_STA_DISCONNECTED)
        self->connected = false;
    if (base == IP_EVENT && id == IP_EVENT_STA_GOT_IP)
        self->connected = true;
}
std::string WifiManager::ipAddress() const {
    esp_netif_ip_info_t info = {};
    auto n = esp_netif_get_handle_from_ifkey("WIFI_STA_DEF");
    if (!connected || !n || esp_netif_get_ip_info(n, &info) != ESP_OK)
        return "0.0.0.0";
    char out[16];
    esp_ip4addr_ntoa(&info.ip, out, sizeof(out));
    return out;
}
