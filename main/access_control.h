#pragma once
#include "esp_http_server.h"
#include <array>
#include <cstdint>
#include <string>
struct WebSession {
    std::string token;
    int64_t expires = 0, admin_until = 0;
    uint32_t sequence = 0;
};
class AccessControl {
  public:
    static AccessControl &getInstance();
    bool init();
    bool verify(const std::string &password, bool admin);
    WebSession *authenticate(httpd_req_t *);
    WebSession *login(bool remember);
    void logout(WebSession &);
    std::string certificate, key;

  private:
    std::string salt, access_hash, admin_hash;
    std::array<WebSession, 4> sessions{};
    int64_t next_attempt = 0;
};
