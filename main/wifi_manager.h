#pragma once
#include <string>
#include "esp_err.h"
#include "esp_event_base.h"

class WifiManager {
public:
    static WifiManager &getInstance();
    esp_err_t init();
    bool isConfigured() const { return configured; }
    bool isConnected() const { return connected; }
    std::string ipAddress() const;
    esp_err_t saveCredentials(const std::string &, const std::string &);
    esp_err_t clearCredentials();
private:
    WifiManager() = default;
    static void eventHandler(void *, esp_event_base_t, int32_t, void *);
    void startAccessPoint();
    void startStation();
    bool loadCredentials();
    std::string ssid, password;
    bool configured = false, connected = false;
    int retry_count = 0;
};
