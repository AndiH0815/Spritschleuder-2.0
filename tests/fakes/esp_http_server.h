#pragma once
#include "esp_err.h"
#include <cstring>
#include <string>
struct httpd_req_t {
    std::string cookie;
};
inline esp_err_t httpd_req_get_hdr_value_str(httpd_req_t *r, const char *, char *out, size_t n) {
    if (r->cookie.empty() || r->cookie.size() + 1 > n)
        return ESP_FAIL;
    strcpy(out, r->cookie.c_str());
    return ESP_OK;
}
