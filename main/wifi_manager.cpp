#include "wifi_manager.h"
#include "arduino_comm.h"
#include "nvs.h"
#ifndef SPRIT_HOST_TEST
#include "esp_event.h"
#include "esp_log.h"
#include "esp_netif.h"
#include "esp_wifi.h"
#include "esp_timer.h"
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "lwip/inet.h"
#endif
#include <algorithm>
#include <cstring>
namespace {
constexpr int64_t FallbackMs = 60000, TestMs = 60000, GraceMs = 15000;
// One NVS value prevents a mismatched SSID/password pair after power loss.
struct Profile { char magic[4], ssid[33], password[65]; };
std::string stationIp() {
    esp_netif_ip_info_t info = {};
    auto n = esp_netif_get_handle_from_ifkey("WIFI_STA_DEF");
    if (!n || esp_netif_get_ip_info(n, &info) != ESP_OK || !info.ip.addr) return "0.0.0.0";
    char out[16]; esp_ip4addr_ntoa(&info.ip, out, sizeof(out)); return out;
}
}
WifiManager &WifiManager::getInstance() { static WifiManager i; return i; }
bool WifiManager::validCredentials(const std::string &s, const std::string &p) {
    if (s.empty() || s.size() > 32 || s.find('\0') != std::string::npos ||
        p.find('\0') != std::string::npos) return false;
    if (p.empty()) return true;
    if (p.size() == 64) return p.find_first_not_of("0123456789abcdefABCDEF") == std::string::npos;
    return p.size() >= 8 && p.size() <= 63;
}
esp_err_t WifiManager::init() {
    nvs_handle_t n;
    if (nvs_open("wifi", NVS_READONLY, &n) == ESP_OK) {
        Profile profile = {}; size_t size = sizeof(profile);
        auto e = nvs_get_blob(n, "profile", &profile, &size);
        if (e == ESP_OK && size == sizeof(profile) && !memcmp(profile.magic, "SW01", 4) &&
            profile.ssid[32] == 0 && profile.password[64] == 0) {
            ssid = profile.ssid; password = profile.password;
        } else if (e == ESP_ERR_NVS_NOT_FOUND) {
            char s[33] = {}, p[65] = {}; size_t sl = sizeof(s), pl = sizeof(p);
            if (nvs_get_str(n, "ssid", s, &sl) == ESP_OK &&
                nvs_get_str(n, "password", p, &pl) == ESP_OK && s[32] == 0 && p[64] == 0) {
                ssid = s; password = p;
            }
        }
        nvs_close(n);
    }
    configured = validCredentials(ssid, password);
    if (!configured) { ssid.clear(); password.clear(); }
    ESP_ERROR_CHECK(esp_netif_init());
    ESP_ERROR_CHECK(esp_event_loop_create_default());
    esp_netif_create_default_wifi_sta();
    auto ap = esp_netif_create_default_wifi_ap();
    esp_netif_ip_info_t ap_ip = {};
    ap_ip.ip.addr = esp_ip4addr_aton("192.168.4.1");
    ap_ip.gw = ap_ip.ip; ap_ip.netmask.addr = esp_ip4addr_aton("255.255.255.0");
    auto stopped = esp_netif_dhcps_stop(ap);
    if (stopped != ESP_ERR_ESP_NETIF_DHCP_ALREADY_STOPPED) ESP_ERROR_CHECK(stopped);
    ESP_ERROR_CHECK(esp_netif_set_ip_info(ap, &ap_ip));
    ESP_ERROR_CHECK(esp_netif_dhcps_start(ap));
    wifi_init_config_t cfg = WIFI_INIT_CONFIG_DEFAULT();
    ESP_ERROR_CHECK(esp_wifi_init(&cfg));
    ESP_ERROR_CHECK(esp_wifi_set_storage(WIFI_STORAGE_RAM));
    ESP_ERROR_CHECK(esp_event_handler_register(WIFI_EVENT, ESP_EVENT_ANY_ID, eventHandler, this));
    ESP_ERROR_CHECK(esp_event_handler_register(IP_EVENT, IP_EVENT_STA_GOT_IP, eventHandler, this));
    ESP_ERROR_CHECK(esp_wifi_set_mode(WIFI_MODE_STA));
    if (configured) {
        wifi_config_t sta = {};
        memcpy(sta.sta.ssid, ssid.data(), ssid.size());
        memcpy(sta.sta.password, password.data(), password.size());
        ESP_ERROR_CHECK(esp_wifi_set_config(WIFI_IF_STA, &sta));
    }
    ESP_ERROR_CHECK(esp_wifi_start());
    offline_since = esp_timer_get_time() / 1000;
    xTaskCreate(retryTask, "wifi_recovery", 4096, this, 3, nullptr);
    return ESP_OK;
}
void WifiManager::eventHandler(void *arg, esp_event_base_t base, int32_t id, void *) {
    auto self = static_cast<WifiManager *>(arg);
    if (base == WIFI_EVENT && (id == WIFI_EVENT_STA_DISCONNECTED || id == WIFI_EVENT_STA_STOP))
        self->got_ip = false;
    if (base == IP_EVENT && id == IP_EVENT_STA_GOT_IP) self->got_ip = true;
}
esp_err_t WifiManager::applyStation(const std::string &s, const std::string &p) {
    esp_wifi_disconnect(); got_ip = false; connected = false; stable_since = -1;
    if (s.empty()) return ESP_OK;
    wifi_config_t sta = {};
    memcpy(sta.sta.ssid, s.data(), s.size()); memcpy(sta.sta.password, p.data(), p.size());
    auto e = esp_wifi_set_config(WIFI_IF_STA, &sta);
    if (e == ESP_OK) e = esp_wifi_connect();
    return e;
}
esp_err_t WifiManager::enableHotspot() {
    wifi_config_t ap = {};
    memcpy(ap.ap.ssid, HotspotName, strlen(HotspotName));
    memcpy(ap.ap.password, HotspotPassword, strlen(HotspotPassword));
    ap.ap.ssid_len = strlen(HotspotName); ap.ap.channel = 1;
    ap.ap.max_connection = 2; ap.ap.authmode = WIFI_AUTH_WPA2_PSK;
    // Configure before restarting, so no default/open AP is advertised.
    esp_err_t e = esp_wifi_stop();
    if (e == ESP_OK) e = esp_wifi_set_mode(WIFI_MODE_APSTA);
    if (e == ESP_OK) e = esp_wifi_set_config(WIFI_IF_AP, &ap);
    got_ip = false; connected = false;
    if (e == ESP_OK) e = esp_wifi_start();
    // STA retry errors must not hide an otherwise working setup AP.
    if (e == ESP_OK && configured) esp_wifi_connect();
    return e;
}
esp_err_t WifiManager::saveCandidate() {
    Profile profile = {}; memcpy(profile.magic, "SW01", 4);
    memcpy(profile.ssid, candidate_ssid.data(), candidate_ssid.size());
    memcpy(profile.password, candidate_password.data(), candidate_password.size());
    nvs_handle_t n; auto e = nvs_open("wifi", NVS_READWRITE, &n);
    if (e != ESP_OK) return e;
    e = nvs_set_blob(n, "profile", &profile, sizeof(profile));
    if (e == ESP_OK) e = nvs_commit(n);
    nvs_close(n); return e;
}
bool WifiManager::requestSetup(bool stopActive) {
    std::lock_guard<std::mutex> lock(mutex);
    if (!ArduinoComm::getInstance().setNetworkSetup(true, stopActive)) return false;
    requested = true; automatic = false; close_at = 0;
    message = "Einrichtungsmodus angefordert; Pumpenstarts gesperrt";
    return true;
}
bool WifiManager::configure(const std::string &s, const std::string &p, std::string &error) {
    std::lock_guard<std::mutex> lock(mutex);
    if (!validCredentials(s, p)) { error = "WLAN-Name oder Passwort ungueltig"; return false; }
    if (pending || testing) { error = "WLAN-Test laeuft bereits"; return false; }
    if (!ArduinoComm::getInstance().setNetworkSetup(true)) {
        error = "Zuerst Ausgabe stoppen"; return false;
    }
    candidate_ssid = s; candidate_password = p; pending = true;
    requested = true; automatic = false; close_at = 0;
    message = "Neue WLAN-Daten werden getestet; bisherige Daten bleiben gespeichert";
    return true;
}
bool WifiManager::finishSetup(std::string &error) {
    std::lock_guard<std::mutex> lock(mutex);
    if (!connected || testing || pending) { error = "Zuerst ein WLAN erfolgreich verbinden"; return false; }
    if (setup || requested) {
        close_at = esp_timer_get_time() / 1000 + 3000;
        message = "Einrichtungs-Hotspot wird geschlossen";
    }
    return true;
}
void WifiManager::tick(int64_t now) {
    std::lock_guard<std::mutex> lock(mutex);
    wifi_ap_record_t record = {};
    const auto &target = testing ? candidate_ssid : ssid;
    bool link = got_ip && !target.empty() && esp_wifi_sta_get_ap_info(&record) == ESP_OK &&
                target == std::string(reinterpret_cast<char *>(record.ssid),
                                      strnlen(reinterpret_cast<char *>(record.ssid), 32)) &&
                stationIp() != "0.0.0.0";
    if (connected && !link) { offline_since = now; close_at = 0; }
    connected = link;
    if (connected) offline_since = now;
    if (!connected && !setup && !requested && now - offline_since >= FallbackMs) {
        ArduinoComm::getInstance().setNetworkSetup(true, true);
        requested = true; automatic = true;
        message = "Kein bekanntes WLAN erreichbar; Einrichtungs-Hotspot";
    }
    if (requested && !setup && !ArduinoComm::getInstance().busy()) {
        if (enableHotspot() != ESP_OK) {
            message = "Einrichtungs-Hotspot konnte nicht gestartet werden"; return;
        }
        setup = true; connected = false; retry_at = now + 5000;
    }
    if (setup && pending && !testing) {
        pending = false; testing = true; test_started = now; stable_since = -1;
        if (applyStation(candidate_ssid, candidate_password) != ESP_OK) test_started = now - TestMs;
        retry_at = now + 5000;
    }
    if (testing) {
        if (!connected) stable_since = -1;
        else if (stable_since < 0) stable_since = now;
        if (stable_since >= 0 && now - stable_since >= 2000) {
            if (saveCandidate() == ESP_OK) {
                ssid = candidate_ssid; password = candidate_password; configured = true;
                testing = false; close_at = now + GraceMs;
                message = "WLAN verbunden und gespeichert. Hotspot schliesst in 15 Sekunden";
            } else {
                testing = false; applyStation(ssid, password);
                message = "Speichern fehlgeschlagen; bisherige WLAN-Daten werden verwendet";
            }
            candidate_ssid.clear(); candidate_password.clear();
        } else if (now - test_started >= TestMs) {
            testing = false; applyStation(ssid, password);
            candidate_ssid.clear(); candidate_password.clear();
            message = "WLAN-Test fehlgeschlagen; bisherige Daten erhalten. Bitte erneut eingeben";
        }
    }
    if (setup && automatic && connected && !close_at) {
        close_at = now + GraceMs; message = "Bekanntes WLAN wieder erreichbar; Hotspot schliesst";
    }
    if (setup && close_at && connected && now >= close_at && !testing && !pending) {
        if (esp_wifi_set_mode(WIFI_MODE_STA) == ESP_OK) {
            setup = requested = automatic = false; close_at = 0;
            ArduinoComm::getInstance().setNetworkSetup(false);
            message = "Mit WLAN verbunden; Einrichtungsmodus beendet";
        }
    }
    if (!connected && (configured || testing) && now >= retry_at) {
        esp_wifi_connect(); retry_at = now + 5000;
    }
}
void WifiManager::retryTask(void *arg) {
    auto self = static_cast<WifiManager *>(arg);
    for (;;) { self->tick(esp_timer_get_time() / 1000); vTaskDelay(pdMS_TO_TICKS(250)); }
}
WifiStatus WifiManager::status() const {
    std::lock_guard<std::mutex> lock(mutex);
    WifiStatus s;
    s.connected = connected; s.configured = configured; s.setup = setup || requested;
    s.testing = testing || pending; s.ssid = testing ? candidate_ssid : ssid;
    s.saved_ssid = ssid; s.message = message; s.ip = connected ? stationIp() : "0.0.0.0";
    return s;
}
std::string WifiManager::ipAddress() const {
    auto s = status(); return s.setup ? "192.168.4.1" : s.ip;
}
