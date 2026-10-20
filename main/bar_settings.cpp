#include "bar_settings.h"
#include "cJSON.h"
#include "nvs.h"
#include <cmath>
namespace {
bool valid(const BarConfiguration &v) {
    if (v.ingredients.size() > 32) return false;
    for (size_t i = 0; i < v.ingredients.size(); ++i) {
        const auto &x = v.ingredients[i];
        if (!x.id || x.id > 255 || x.name.empty() || x.name.size() > 48) return false;
        for (size_t j = 0; j < i; ++j)
            if (v.ingredients[j].id == x.id || v.ingredients[j].name == x.name) return false;
    }
    for (const auto &g : v.glasses)
        if (g.name.empty() || g.name.size() > 24 || !std::isfinite(g.ml) || g.ml < 20 || g.ml > 1000)
            return false;
    return true;
}
}
BarSettings &BarSettings::getInstance() { static BarSettings v; return v; }
BarConfiguration BarSettings::all() const {
    std::lock_guard<std::mutex> lock(mutex); return configuration;
}
esp_err_t BarSettings::init() {
    nvs_handle_t n;
    auto e = nvs_open("bar", NVS_READONLY, &n);
    if (e == ESP_ERR_NVS_NOT_FOUND) return ESP_OK;
    if (e != ESP_OK) return e;
    size_t size = 0;
    e = nvs_get_str(n, "v1", nullptr, &size);
    if (e == ESP_ERR_NVS_NOT_FOUND) { nvs_close(n); return ESP_OK; }
    if (e != ESP_OK || size > 8192) { nvs_close(n); return e == ESP_OK ? ESP_ERR_INVALID_SIZE : e; }
    std::string text(size, '\0');
    e = nvs_get_str(n, "v1", &text[0], &size); nvs_close(n);
    if (e != ESP_OK) return e;
    auto root = cJSON_Parse(text.c_str());
    if (!root) return ESP_ERR_INVALID_STATE;
    BarConfiguration next;
    auto list = cJSON_GetObjectItemCaseSensitive(root, "ingredients");
    auto glasses = cJSON_GetObjectItemCaseSensitive(root, "glasses");
    bool good = cJSON_IsArray(list) && cJSON_GetArraySize(list) <= 32 &&
                cJSON_IsArray(glasses) && cJSON_GetArraySize(glasses) == 2;
    if (good) {
        cJSON *p = nullptr;
        cJSON_ArrayForEach(p, list) {
            auto id = cJSON_GetObjectItemCaseSensitive(p, "id");
            auto name = cJSON_GetObjectItemCaseSensitive(p, "name");
            if (!cJSON_IsNumber(id) || !std::isfinite(id->valuedouble) ||
                id->valuedouble < 1 || id->valuedouble > 255 ||
                std::floor(id->valuedouble) != id->valuedouble || !cJSON_IsString(name)) {
                good = false; break;
            }
            next.ingredients.push_back({uint16_t(id->valuedouble), name->valuestring});
        }
        for (int i = 0; i < 2; ++i) {
            auto g = cJSON_GetArrayItem(glasses, i);
            auto name = cJSON_GetObjectItemCaseSensitive(g, "name");
            auto ml = cJSON_GetObjectItemCaseSensitive(g, "ml");
            if (!cJSON_IsString(name) || !cJSON_IsNumber(ml)) { good = false; break; }
            next.glasses[i] = {name->valuestring, ml->valuedouble};
        }
    }
    cJSON_Delete(root);
    if (!good || !valid(next)) return ESP_ERR_INVALID_STATE;
    std::lock_guard<std::mutex> lock(mutex); configuration = next;
    return ESP_OK;
}
esp_err_t BarSettings::replace(const BarConfiguration &next) {
    if (!valid(next)) return ESP_ERR_INVALID_ARG;
    auto root = cJSON_CreateObject();
    auto ingredients = cJSON_AddArrayToObject(root, "ingredients");
    for (const auto &x : next.ingredients) {
        auto item = cJSON_CreateObject(); cJSON_AddItemToArray(ingredients, item);
        cJSON_AddNumberToObject(item, "id", x.id);
        cJSON_AddStringToObject(item, "name", x.name.c_str());
    }
    auto glasses = cJSON_AddArrayToObject(root, "glasses");
    for (const auto &g : next.glasses) {
        auto item = cJSON_CreateObject(); cJSON_AddItemToArray(glasses, item);
        cJSON_AddStringToObject(item, "name", g.name.c_str());
        cJSON_AddNumberToObject(item, "ml", g.ml);
    }
    char *text = cJSON_PrintUnformatted(root); cJSON_Delete(root);
    if (!text) return ESP_ERR_NO_MEM;
    std::lock_guard<std::mutex> lock(mutex);
    nvs_handle_t n;
    auto e = nvs_open("bar", NVS_READWRITE, &n);
    if (e == ESP_OK) {
        e = nvs_set_str(n, "v1", text);
        if (e == ESP_OK) e = nvs_commit(n);
        nvs_close(n);
    }
    cJSON_free(text);
    if (e == ESP_OK) configuration = next;
    return e;
}
