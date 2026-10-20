#include "photo_store.h"
#include "recipes.h"
#include "esp_partition.h"
#include "esp_random.h"
#include "esp_spiffs.h"
#include <memory>
#include <new>
#include <cstdio>
#include <cstring>
#include <dirent.h>
#include <sys/stat.h>
PhotoStore &PhotoStore::getInstance() { static PhotoStore v; return v; }
bool PhotoStore::validKey(const std::string &key) {
    if (key.size() != 16) return false;
    for (char c : key)
        if (!((c >= '0' && c <= '9') || (c >= 'a' && c <= 'f'))) return false;
    return true;
}
std::string PhotoStore::path(const std::string &key) {
    return validKey(key) ? "/photos/" + key + ".jpg" : "";
}
void PhotoStore::remove(const std::string &key) {
    if (mounted && validKey(key)) std::remove(path(key).c_str());
}
std::string PhotoStore::freshKey() const {
    if (!mounted) return "";
    for (int i = 0; i < 8; ++i) {
        char key[17]; snprintf(key, sizeof(key), "%08lx%08lx",
                               (unsigned long)esp_random(), (unsigned long)esp_random());
        struct stat info;
        if (stat(path(key).c_str(), &info) != 0) return key;
    }
    return "";
}
esp_err_t PhotoStore::init() {
    esp_vfs_spiffs_conf_t cfg{};
    cfg.base_path = "/photos"; cfg.partition_label = "photos";
    cfg.max_files = 3; cfg.format_if_mount_failed = false;
    auto e = esp_vfs_spiffs_register(&cfg);
    if (e != ESP_OK) {
        // Format only a verified completely erased partition, never an existing
        // or damaged file system. Do this before pump communications start.
        auto part = esp_partition_find_first(ESP_PARTITION_TYPE_DATA,
                                             ESP_PARTITION_SUBTYPE_DATA_SPIFFS, "photos");
        if (!part || e != ESP_FAIL) return e;
        // app_main has a small stack; keep the flash scan buffer on the heap.
        constexpr size_t chunk = 4096;
        std::unique_ptr<unsigned char[]> bytes(new (std::nothrow) unsigned char[chunk]);
        if (!bytes) return ESP_ERR_NO_MEM;
        for (size_t offset = 0; offset < part->size; offset += chunk) {
            size_t count = part->size - offset;
            if (count > chunk) count = chunk;
            e = esp_partition_read(part, offset, bytes.get(), count);
            if (e != ESP_OK) return e;
            for (size_t j = 0; j < count; ++j)
                if (bytes[j] != 0xff) return ESP_ERR_INVALID_STATE;
        }
        e = esp_spiffs_format("photos");
        if (e != ESP_OK) return e;
        e = esp_vfs_spiffs_register(&cfg);
        if (e != ESP_OK) return e;
    }
    mounted = true;
    // A power loss before committing the recipe can leave a new unreferenced
    // image. Retain every referenced image; collect only our own unused names.
    auto recipes = RecipeStore::getInstance().all();
    DIR *dir = opendir("/photos");
    if (dir) {
        while (auto entry = readdir(dir)) {
            std::string name = entry->d_name;
            if (name.size() != 20 || name.substr(16) != ".jpg") continue;
            auto key = name.substr(0, 16);
            if (!validKey(key)) continue;
            bool used = false;
            for (const auto &r : recipes) if (r.photo_key == key) used = true;
            if (!used) remove(key);
        }
        closedir(dir);
    }
    return ESP_OK;
}
