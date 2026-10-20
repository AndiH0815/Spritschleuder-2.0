#pragma once
#include "esp_err.h"
#ifndef SPRIT_HOST_TEST
#include "esp_event_base.h"
#else
#include "../tests/fakes/wifi_platform.h"
#endif
#include <atomic>
#include <mutex>
#include <string>
struct WifiStatus {
    bool connected = false, configured = false, setup = false, testing = false;
    std::string ssid, saved_ssid, ip, message;
};
class WifiManager {
  public:
    static constexpr const char *HotspotName = "spritschleuder";
    static constexpr const char *HotspotPassword = "12345678";
    static WifiManager &getInstance();
    esp_err_t init();
    WifiStatus status() const;
    std::string ipAddress() const;
    bool requestSetup(bool stopActive = false);
    bool configure(const std::string &ssid, const std::string &password, std::string &error);
    bool finishSetup(std::string &error);
    static bool validCredentials(const std::string &, const std::string &);
  private:
    friend struct WifiTest;
    static void eventHandler(void *, esp_event_base_t, int32_t, void *);
    static void retryTask(void *);
    void tick(int64_t now);
    esp_err_t applyStation(const std::string &, const std::string &);
    esp_err_t enableHotspot();
    esp_err_t saveCandidate();
    mutable std::mutex mutex;
    std::atomic<bool> got_ip{false};
    bool configured = false, connected = false, setup = false, requested = false;
    bool automatic = false, pending = false, testing = false;
    std::string ssid, password, candidate_ssid, candidate_password, message;
    int64_t offline_since = 0, retry_at = 0, test_started = 0, stable_since = -1, close_at = 0;
};
