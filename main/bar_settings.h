#pragma once
#include "esp_err.h"
#include <array>
#include <cstdint>
#include <mutex>
#include <string>
#include <vector>
struct IngredientName { uint16_t id = 0; std::string name; };
struct GlassProfile { std::string name; double ml = 400; };
struct BarConfiguration {
    std::vector<IngredientName> ingredients;
    std::array<GlassProfile, 2> glasses{{{"Klein", 200}, {"Gross", 400}}};
};
class BarSettings {
  public:
    static BarSettings &getInstance();
    esp_err_t init();
    BarConfiguration all() const;
    esp_err_t replace(const BarConfiguration &);
  private:
    mutable std::mutex mutex;
    BarConfiguration configuration;
};
