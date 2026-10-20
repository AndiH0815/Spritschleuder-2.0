#pragma once
#include "esp_err.h"

// Lifecycle belongs to the HTTPS server; credentials are never advertised.
class LocalDiscovery {
  public:
    static constexpr const char *Hostname = "spritschleuder";
    static constexpr const char *Url = "https://spritschleuder.local";
    static esp_err_t start();
    static void stop();
    static bool running();
};
