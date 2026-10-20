#include "recipes.h"
#include "cJSON.h"
#include "config.h"
#include "nvs.h"
#include <cmath>
#include <cstring>
namespace {
struct LegacyRecipe {
    char name[25];
    uint16_t amount_ml[4];
};
} // namespace
RecipeStore &RecipeStore::getInstance() {
    static RecipeStore i;
    return i;
}
esp_err_t RecipeStore::init() {
    std::lock_guard<std::mutex> l(mutex);
    nvs_handle_t n;
    auto e = nvs_open("recipes", NVS_READONLY, &n);
    if (e == ESP_ERR_NVS_NOT_FOUND)
        return ESP_OK;
    if (e != ESP_OK)
        return e;
    size_t size = 0;
    e = nvs_get_str(n, "v2", nullptr, &size);
    if (e == ESP_OK) {
        if (size > 16384) {
            nvs_close(n);
            return ESP_ERR_INVALID_SIZE;
        }
        std::string text(size, '\0');
        e = nvs_get_str(n, "v2", &text[0], &size);
        nvs_close(n);
        if (e != ESP_OK)
            return e;
        auto root = cJSON_Parse(text.c_str());
        if (!root)
            return ESP_ERR_INVALID_STATE;
        auto list = cJSON_GetObjectItemCaseSensitive(root, "recipes"),
             next = cJSON_GetObjectItemCaseSensitive(root, "next_id");
        bool valid = cJSON_IsArray(list) && cJSON_GetArraySize(list) <= MAX_COCKTAILS &&
                     cJSON_IsNumber(next) && next->valuedouble >= 1 &&
                     next->valuedouble <= UINT32_MAX;
        if (valid)
            next_id = uint32_t(next->valuedouble);
        cJSON *item = nullptr;
        cJSON_ArrayForEach(item, list) {
            auto id = cJSON_GetObjectItemCaseSensitive(item, "id"),
                 name = cJSON_GetObjectItemCaseSensitive(item, "name"),
                 manual = cJSON_GetObjectItemCaseSensitive(item, "manual"),
                 portions = cJSON_GetObjectItemCaseSensitive(item, "portions");
            if (!valid || !cJSON_IsNumber(id) || id->valuedouble < 1 ||
                id->valuedouble >= next_id || !cJSON_IsString(name) || !cJSON_IsString(manual) ||
                !cJSON_IsArray(portions) || cJSON_GetArraySize(portions) > 8) {
                valid = false;
                break;
            }
            Recipe r;
            r.id = uint32_t(id->valuedouble);
            r.name = name->valuestring;
            r.manual_name = manual->valuestring;
            r.needs_review = cJSON_IsTrue(cJSON_GetObjectItemCaseSensitive(item, "review"));
            cJSON *p = nullptr;
            cJSON_ArrayForEach(p, portions) {
                auto ingredient = cJSON_GetObjectItemCaseSensitive(p, "ingredient"),
                     ml = cJSON_GetObjectItemCaseSensitive(p, "ml");
                if (!cJSON_IsNumber(ingredient) || ingredient->valuedouble < 1 ||
                    ingredient->valuedouble > 255 || !cJSON_IsNumber(ml) || ml->valuedouble <= 0 ||
                    ml->valuedouble > 400) {
                    valid = false;
                    break;
                }
                r.dosing.portions[r.dosing.count++] = {
                    uint16_t(ingredient->valuedouble), ml->valuedouble,
                    bool(cJSON_IsTrue(cJSON_GetObjectItemCaseSensitive(p, "strength")))};
            }
            for (auto &existing : recipes)
                if (existing.id == r.id)
                    valid = false;
            if (r.name.empty() || r.name.size() > 64 || r.manual_name.size() > 64 ||
                !r.dosing.count)
                valid = false;
            if (valid)
                recipes.push_back(r);
        }
        cJSON_Delete(root);
        return valid ? ESP_OK : ESP_ERR_INVALID_STATE;
    }
    if (e != ESP_ERR_NVS_NOT_FOUND) {
        nvs_close(n);
        return e;
    }
    size = 0;
    e = nvs_get_blob(n, "items", nullptr, &size);
    if (e == ESP_ERR_NVS_NOT_FOUND) {
        nvs_close(n);
        return ESP_OK;
    }
    if (e != ESP_OK || !size || size % sizeof(LegacyRecipe) ||
        size / sizeof(LegacyRecipe) > MAX_COCKTAILS) {
        nvs_close(n);
        return ESP_ERR_INVALID_STATE;
    }
    std::vector<LegacyRecipe> old(size / sizeof(LegacyRecipe));
    e = nvs_get_blob(n, "items", old.data(), &size);
    nvs_close(n);
    if (e != ESP_OK)
        return e;
    for (const auto &v : old) {
        Recipe r;
        r.id = next_id++;
        r.name.assign(v.name, strnlen(v.name, sizeof(v.name)));
        r.needs_review = true;
        // Preserve old amounts. Do not reinterpret an old pump-only ratio as a
        // 400ml finished cocktail.
        for (uint16_t i = 0; i < 4; ++i)
            if (v.amount_ml[i])
                r.dosing.portions[r.dosing.count++] = {uint16_t(i + 1), double(v.amount_ml[i]),
                                                       false};
        if (!r.name.empty() && r.dosing.count)
            recipes.push_back(r);
    }
    // Leave the original "items" blob untouched for backup/downgrade.
    return save();
}
std::vector<Recipe> RecipeStore::all() const {
    std::lock_guard<std::mutex> l(mutex);
    return recipes;
}
bool RecipeStore::get(uint32_t id, Recipe &r) const {
    std::lock_guard<std::mutex> l(mutex);
    for (auto &v : recipes)
        if (v.id == id) {
            r = v;
            return true;
        }
    return false;
}
esp_err_t RecipeStore::put(Recipe &r) {
    if (r.name.empty() || r.name.size() > 64 || r.manual_name.size() > 64 || !r.dosing.count ||
        r.dosing.count > 8)
        return ESP_ERR_INVALID_ARG;
    double total = 0;
    for (size_t i = 0; i < r.dosing.count; ++i) {
        auto &p = r.dosing.portions[i];
        if (!p.ingredient || p.ingredient > 255 || !std::isfinite(p.ml) || p.ml <= 0 || p.ml > 400)
            return ESP_ERR_INVALID_ARG;
        total += p.ml;
        for (size_t j = 0; j < i; ++j)
            if (r.dosing.portions[j].ingredient == p.ingredient)
                return ESP_ERR_INVALID_ARG;
    }
    if (total > 400)
        return ESP_ERR_INVALID_ARG;
    std::lock_guard<std::mutex> l(mutex);
    auto previous = recipes;
    auto old_next = next_id;
    if (!r.id) {
        if (recipes.size() >= MAX_COCKTAILS || next_id == UINT32_MAX)
            return ESP_ERR_NO_MEM;
        r.id = next_id++;
        recipes.push_back(r);
    } else {
        bool found = false;
        for (auto &v : recipes)
            if (v.id == r.id) {
                v = r;
                found = true;
                break;
            }
        if (!found)
            return ESP_ERR_NOT_FOUND;
    }
    auto e = save();
    if (e != ESP_OK) {
        recipes = previous;
        next_id = old_next;
    }
    return e;
}
esp_err_t RecipeStore::remove(uint32_t id) {
    std::lock_guard<std::mutex> l(mutex);
    auto before = recipes;
    for (auto it = recipes.begin(); it != recipes.end(); ++it)
        if (it->id == id) {
            recipes.erase(it);
            auto e = save();
            if (e != ESP_OK)
                recipes = before;
            return e;
        }
    return ESP_ERR_NOT_FOUND;
}
esp_err_t RecipeStore::save() {
    auto root = cJSON_CreateObject();
    cJSON_AddNumberToObject(root, "next_id", next_id);
    auto list = cJSON_AddArrayToObject(root, "recipes");
    for (auto &r : recipes) {
        auto v = cJSON_CreateObject();
        cJSON_AddItemToArray(list, v);
        cJSON_AddNumberToObject(v, "id", r.id);
        cJSON_AddStringToObject(v, "name", r.name.c_str());
        cJSON_AddStringToObject(v, "manual", r.manual_name.c_str());
        cJSON_AddBoolToObject(v, "review", r.needs_review);
        auto ps = cJSON_AddArrayToObject(v, "portions");
        for (size_t i = 0; i < r.dosing.count; ++i) {
            auto p = cJSON_CreateObject();
            cJSON_AddItemToArray(ps, p);
            auto &part = r.dosing.portions[i];
            cJSON_AddNumberToObject(p, "ingredient", part.ingredient);
            cJSON_AddNumberToObject(p, "ml", part.ml);
            cJSON_AddBoolToObject(p, "strength", part.strength);
        }
    }
    char *json = cJSON_PrintUnformatted(root);
    cJSON_Delete(root);
    if (!json)
        return ESP_ERR_NO_MEM;
    nvs_handle_t n;
    auto e = nvs_open("recipes", NVS_READWRITE, &n);
    if (e == ESP_OK) {
        e = nvs_set_str(n, "v2", json);
        if (e == ESP_OK)
            e = nvs_commit(n);
        nvs_close(n);
    }
    cJSON_free(json);
    return e;
}
