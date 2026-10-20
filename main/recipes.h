#pragma once
#include "../shared/dosing.h"
#include "esp_err.h"
#include <array>
#include <mutex>
#include <string>
#include <vector>
struct Recipe {
    uint32_t id = 0;
    std::string name, manual_name = "Manuell auffuellen";
    sprit::DosingRecipe dosing;
    bool needs_review = false;
};
class RecipeStore {
  public:
    static RecipeStore &getInstance();
    esp_err_t init();
    std::vector<Recipe> all() const;
    bool get(uint32_t id, Recipe &) const;
    esp_err_t put(Recipe &);
    esp_err_t remove(uint32_t id);

  private:
    esp_err_t save();
    mutable std::mutex mutex;
    std::vector<Recipe> recipes;
    uint32_t next_id = 1;
};
