#include "../main/pump_settings.h"
#include "../main/recipes.h"
#include "fakes/nvs.h"
#include <cassert>
#include <iostream>
int main() {
    struct Old {
        char name[25];
        uint16_t ml[4];
    };
    Old legacy[] = {{"Alt-Cocktail", {50, 10, 100, 0}}, {"Anderer", {20, 0, 0, 30}}};
    nvs_set_blob("recipes", "items", legacy, sizeof(legacy));
    auto old_blob = fake_nvs["recipes"]["items"];
    RecipeStore first;
    assert(first.init() == ESP_OK);
    auto recipes = first.all();
    assert(recipes.size() == 2 && recipes[0].needs_review &&
           recipes[0].dosing.portions[0].ml == 50 && recipes[0].dosing.portions[2].ingredient == 3);
    assert(fake_nvs["recipes"]["items"] == old_blob);
    auto recipe = recipes[0];
    recipe.needs_review = false;
    recipe.name = "Cuba Libre";
    recipe.dosing.count = 2;
    recipe.dosing.portions[0] = {1, 40, true};
    recipe.dosing.portions[1] = {4, 20, false};
    assert(first.put(recipe) == ESP_OK);
    assert(first.remove(recipes[1].id) == ESP_OK);
    Recipe added = recipe;
    added.id = 0;
    added.name = "Neu";
    assert(first.put(added) == ESP_OK && added.id > recipes[1].id);
    fail_commit = true;
    recipe.name = "Darf nicht bleiben";
    assert(first.put(recipe) != ESP_OK);
    Recipe after;
    assert(first.get(recipe.id, after) && after.name == "Cuba Libre");
    assert(first.remove(recipe.id) != ESP_OK && first.get(recipe.id, after));
    fail_commit = false;
    RecipeStore reboot;
    assert(reboot.init() == ESP_OK);
    assert(reboot.all().size() == 2 && reboot.get(added.id, after) && after.name == "Neu");
    assert(fake_nvs["recipes"]["items"] == old_blob);
    PumpSettings settings;
    assert(settings.init() == ESP_OK);
    auto pumps = settings.all();
    for (auto &p : pumps)
        assert(!p.active && p.ml_per_second == 0);
    pumps[7] = {true, 4, 2.5};
    assert(settings.replace(pumps) == ESP_OK);
    pumps[7].active = false;
    assert(settings.replace(pumps) == ESP_OK);
    PumpSettings new_settings;
    assert(new_settings.init() == ESP_OK);
    assert(!new_settings.all()[7].active && new_settings.all()[7].ingredient == 4 &&
           new_settings.all()[7].ml_per_second == 2.5);
    pumps[0] = {true, 4, 2};
    pumps[7].active = true;
    assert(settings.replace(pumps) == ESP_ERR_INVALID_ARG);
    pumps[7].active = false;
    fail_commit = true;
    assert(settings.replace(pumps) != ESP_OK && !settings.all()[0].active);
    fail_commit = false;
    fake_nvs["recipes"]["v2"] = {'{', '!', '}', 0};
    RecipeStore broken;
    assert(broken.init() != ESP_OK);
    std::cout << "NVS migration, stable IDs, reboot, disabled settings retention, invalid storage, "
                 "failed saves: PASS\n";
}
