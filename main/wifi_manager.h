#pragma once
#include "esp_err.h"
#include "esp_event_base.h"
#include <atomic>
#include <string>
class WifiManager {
  public:
    static WifiManager &getInstance();
    esp_err_t init();
    bool isConfigured() const { return configured; }
    bool isConnected() const { return connected.load(); }
    std::string ipAddress() const;

  private:
    static void eventHandler(void *, esp_event_base_t, int32_t, void *);
    static void retryTask(void *);
    std::string ssid, password;
    bool configured = false;
    std::atomic<bool> connected{false};
};
