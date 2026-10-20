#pragma once
#include "esp_http_server.h"
class CocktailServer {
  public:
    static CocktailServer &getInstance();
    esp_err_t start();
    void stop();

  private:
    static esp_err_t pageHandler(httpd_req_t *);
    static esp_err_t apiHandler(httpd_req_t *);
    httpd_handle_t server = nullptr;
};
