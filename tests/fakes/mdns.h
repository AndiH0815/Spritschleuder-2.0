#pragma once
#include "esp_err.h"
#include <cassert>
#include <cstddef>
#include <cstdint>
#include <string>
#include <vector>

inline int fake_mdns_fail_at = 0, fake_mdns_calls = 0, fake_mdns_frees = 0;
inline std::string fake_mdns_host, fake_mdns_instance;
struct FakeMdnsService { std::string service, protocol; uint16_t port; };
inline std::vector<FakeMdnsService> fake_mdns_services;
inline esp_err_t mdns_result() { return ++fake_mdns_calls == fake_mdns_fail_at ? ESP_FAIL : ESP_OK; }
inline esp_err_t mdns_init() { return mdns_result(); }
inline esp_err_t mdns_hostname_set(const char *host) { fake_mdns_host = host; return mdns_result(); }
inline esp_err_t mdns_instance_name_set(const char *instance) { fake_mdns_instance = instance; return mdns_result(); }
inline esp_err_t mdns_service_add(const char *instance, const char *service, const char *protocol,
                                  uint16_t port, const void *txt, size_t count) {
    assert(!instance && !txt && !count); // No credentials or session data in TXT.
    fake_mdns_services.push_back({service, protocol, port});
    return mdns_result();
}
inline void mdns_free() { ++fake_mdns_frees; fake_mdns_services.clear(); }
