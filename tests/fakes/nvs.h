#pragma once
#include "esp_err.h"
#include <cstring>
#include <map>
#include <string>
#include <vector>
using nvs_handle_t = std::string;
constexpr int NVS_READONLY = 0, NVS_READWRITE = 1;
inline std::map<std::string, std::map<std::string, std::vector<unsigned char>>> fake_nvs;
inline bool fail_commit = false;
inline esp_err_t nvs_open(const char *n, int mode, nvs_handle_t *out) {
    if (mode == NVS_READONLY && !fake_nvs.count(n))
        return ESP_ERR_NVS_NOT_FOUND;
    *out = n;
    return ESP_OK;
}
inline void nvs_close(nvs_handle_t) {}
inline esp_err_t nvs_get_blob(nvs_handle_t n, const char *k, void *out, size_t *size) {
    auto &items = fake_nvs[n];
    if (!items.count(k))
        return ESP_ERR_NVS_NOT_FOUND;
    auto &b = items[k];
    if (out && *size < b.size())
        return ESP_ERR_INVALID_SIZE;
    if (out)
        memcpy(out, b.data(), b.size());
    *size = b.size();
    return ESP_OK;
}
inline esp_err_t nvs_get_str(nvs_handle_t n, const char *k, char *out, size_t *size) {
    return nvs_get_blob(n, k, out, size);
}
inline esp_err_t nvs_set_blob(nvs_handle_t n, const char *k, const void *p, size_t size) {
    if (fail_commit)
        return ESP_FAIL;
    auto b = static_cast<const unsigned char *>(p);
    fake_nvs[n][k] = {b, b + size};
    return ESP_OK;
}
inline esp_err_t nvs_set_str(nvs_handle_t n, const char *k, const char *p) {
    return nvs_set_blob(n, k, p, strlen(p) + 1);
}
inline esp_err_t nvs_commit(nvs_handle_t) { return fail_commit ? ESP_FAIL : ESP_OK; }
