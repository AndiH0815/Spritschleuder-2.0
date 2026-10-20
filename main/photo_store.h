#pragma once
#include "esp_err.h"
#include <string>
class PhotoStore {
  public:
    static PhotoStore &getInstance();
    esp_err_t init();
    bool ready() const { return mounted; }
    std::string freshKey() const;
    static bool validKey(const std::string &);
    static std::string path(const std::string &);
    void remove(const std::string &);
  private:
    bool mounted = false;
};
