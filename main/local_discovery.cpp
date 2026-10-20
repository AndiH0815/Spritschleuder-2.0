#include "local_discovery.h"
#include "config.h"
#include "mdns.h"
#include <atomic>
#include <mutex>

namespace {
std::mutex mutex;
std::atomic<bool> active{false};
}
esp_err_t LocalDiscovery::start() {
    std::lock_guard<std::mutex> lock(mutex);
    if (active) return ESP_OK;
    auto error = mdns_init();
    if (error != ESP_OK) return error;
    error = mdns_hostname_set(Hostname);
    if (error == ESP_OK) error = mdns_instance_name_set("Spritschleuder");
    // Default STA/AP interfaces track DHCP changes and hotspot transitions.
    // The HTTPS service is published only after the server has started.
    if (error == ESP_OK)
        error = mdns_service_add(nullptr, "_https", "_tcp", HTTP_PORT, nullptr, 0);
    if (error != ESP_OK) {
        mdns_free();
        return error;
    }
    active = true;
    return ESP_OK;
}
void LocalDiscovery::stop() {
    std::lock_guard<std::mutex> lock(mutex);
    if (active) { mdns_free(); active = false; }
}
bool LocalDiscovery::running() { return active; }
