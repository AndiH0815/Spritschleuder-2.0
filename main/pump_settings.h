#pragma once
#include "../shared/dosing.h"
#include "esp_err.h"
#include <array>
#include <mutex>
class PumpSettings {
  public:
    static PumpSettings &getInstance();
    esp_err_t init();
    std::array<sprit::Pump, sprit::PumpCount> all();
    esp_err_t replace(const std::array<sprit::Pump, sprit::PumpCount> &);

  private:
    std::mutex mutex;
    std::array<sprit::Pump, sprit::PumpCount> pumps{};
};
