#include "access_control.h"
#include "esp_random.h"
#include "esp_timer.h"
#include "nvs.h"
#include "psa/crypto.h"
#include <cstring>
namespace {
std::string read(nvs_handle_t n, const char *key) {
    size_t size = 0;
    if (nvs_get_str(n, key, nullptr, &size) != ESP_OK || size < 1 || size > 8192)
        return {};
    std::string s(size, '\0');
    if (nvs_get_str(n, key, &s[0], &size) != ESP_OK)
        return {};
    s.resize(size - 1);
    return s;
}
std::string hex(const unsigned char *p, size_t n) {
    const char *digits = "0123456789abcdef";
    std::string s;
    for (size_t i = 0; i < n; ++i) {
        s += digits[p[i] >> 4];
        s += digits[p[i] & 15];
    }
    return s;
}
bool equal(const std::string &a, const std::string &b) {
    if (a.size() != b.size())
        return false;
    unsigned char d = 0;
    for (size_t i = 0; i < a.size(); ++i)
        d |= a[i] ^ b[i];
    return !d;
}
} // namespace
AccessControl &AccessControl::getInstance() {
    static AccessControl i;
    return i;
}
bool AccessControl::init() {
    nvs_handle_t n;
    if (nvs_open("web_access", NVS_READONLY, &n) != ESP_OK)
        return false;
    salt = read(n, "salt");
    access_hash = read(n, "access_hash");
    admin_hash = read(n, "admin_hash");
    certificate = read(n, "certificate");
    key = read(n, "private_key");
    nvs_close(n);
    return salt.size() == 32 && access_hash.size() == 64 && admin_hash.size() == 64 &&
           !certificate.empty() && !key.empty();
}
bool AccessControl::verify(const std::string &password, bool admin) {
    const int64_t now = esp_timer_get_time();
    if (now < next_attempt || password.empty() || password.size() > 128)
        return false;
    next_attempt = now + 1000000;
    unsigned char hash[32];
    // Same PBKDF2 parameters as tools/provision.py. Salt is random hex text.
    if (psa_crypto_init() != PSA_SUCCESS)
        return false;
    psa_key_derivation_operation_t op = PSA_KEY_DERIVATION_OPERATION_INIT;
    psa_status_t result = psa_key_derivation_setup(&op, PSA_ALG_PBKDF2_HMAC(PSA_ALG_SHA_256));
    if (result == PSA_SUCCESS)
        result = psa_key_derivation_input_integer(&op, PSA_KEY_DERIVATION_INPUT_COST, 100000);
    if (result == PSA_SUCCESS)
        result = psa_key_derivation_input_bytes(
            &op, PSA_KEY_DERIVATION_INPUT_SALT,
            reinterpret_cast<const unsigned char *>(salt.data()), salt.size());
    if (result == PSA_SUCCESS)
        result = psa_key_derivation_input_bytes(
            &op, PSA_KEY_DERIVATION_INPUT_PASSWORD,
            reinterpret_cast<const unsigned char *>(password.data()), password.size());
    if (result == PSA_SUCCESS)
        result = psa_key_derivation_output_bytes(&op, hash, sizeof(hash));
    psa_key_derivation_abort(&op);
    if (result != PSA_SUCCESS)
        return false;
    return equal(hex(hash, 32), admin ? admin_hash : access_hash);
}
WebSession *AccessControl::login(bool remember) {
    auto now = esp_timer_get_time();
    for (auto &s : sessions)
        if (s.token.empty() || s.expires <= now) {
            unsigned char random[32];
            esp_fill_random(random, sizeof(random));
            s = {};
            s.token = hex(random, sizeof(random));
            s.expires = now + int64_t(remember ? 30 * 86400 : 12 * 3600) * 1000000;
            return &s;
        }
    return nullptr;
}
WebSession *AccessControl::authenticate(httpd_req_t *r) {
    char header[256];
    if (httpd_req_get_hdr_value_str(r, "Cookie", header, sizeof(header)) != ESP_OK)
        return nullptr;
    const std::string cookies = header;
    const auto now = esp_timer_get_time();
    size_t at = 0;
    while (at < cookies.size()) {
        auto end = cookies.find(';', at);
        auto part = cookies.substr(at, end == std::string::npos ? end : end - at);
        while (!part.empty() && part[0] == ' ')
            part.erase(0, 1);
        if (part.compare(0, 6, "sprit=") == 0) {
            auto token = part.substr(6);
            for (auto &s : sessions)
                if (s.expires > now && equal(s.token, token))
                    return &s;
            return nullptr;
        }
        if (end == std::string::npos)
            break;
        at = end + 1;
    }
    return nullptr;
}
void AccessControl::logout(WebSession &s) { s = {}; }
