#include "pump_settings.h"
#include "nvs.h"
#include <cmath>
namespace {
struct Stored {
    uint32_t version;
    uint8_t active[8];
    uint16_t ingredients[8];
    double rates[8];
};
} // namespace
PumpSettings &PumpSettings::getInstance() {
    static PumpSettings i;
    return i;
}
esp_err_t PumpSettings::init() {
    nvs_handle_t n;
    auto e = nvs_open("pump_settings", NVS_READONLY, &n);
    if (e == ESP_ERR_NVS_NOT_FOUND)
        return ESP_OK;
    if (e != ESP_OK)
        return e;
    Stored s{};
    size_t size = sizeof(s);
    e = nvs_get_blob(n, "v1", &s, &size);
    nvs_close(n);
    if (e == ESP_ERR_NVS_NOT_FOUND)
        return ESP_OK;
    if (e != ESP_OK || size != sizeof(s) || s.version != 1)
        return ESP_ERR_INVALID_STATE;
    for (size_t i = 0; i < 8; ++i) {
        if (s.active[i] > 1 || !std::isfinite(s.rates[i]) || s.rates[i] < 0 || s.rates[i] > 100 ||
            s.ingredients[i] > 255)
            return ESP_ERR_INVALID_STATE;
        pumps[i] = {bool(s.active[i]), s.ingredients[i], s.rates[i]};
    }
    return ESP_OK;
}
std::array<sprit::Pump, sprit::PumpCount> PumpSettings::all() {
    std::lock_guard<std::mutex> l(mutex);
    return pumps;
}
esp_err_t PumpSettings::replace(const std::array<sprit::Pump, sprit::PumpCount> &p) {
    Stored s{};
    s.version = 1;
    for (size_t i = 0; i < 8; ++i) {
        if (p[i].ingredient > 255 || (p[i].active && !p[i].ingredient) ||
            !std::isfinite(p[i].ml_per_second) || p[i].ml_per_second < 0 ||
            p[i].ml_per_second > 100)
            return ESP_ERR_INVALID_ARG;
        for (size_t j = 0; j < i; ++j)
            if (p[i].active && p[j].active && p[j].ingredient == p[i].ingredient)
                return ESP_ERR_INVALID_ARG;
        s.active[i] = p[i].active;
        s.ingredients[i] = p[i].ingredient;
        s.rates[i] = p[i].ml_per_second;
    }
    std::lock_guard<std::mutex> l(mutex);
    nvs_handle_t n;
    auto e = nvs_open("pump_settings", NVS_READWRITE, &n);
    if (e != ESP_OK)
        return e;
    e = nvs_set_blob(n, "v1", &s, sizeof(s));
    if (e == ESP_OK)
        e = nvs_commit(n);
    nvs_close(n);
    if (e == ESP_OK)
        pumps = p;
    return e;
}
