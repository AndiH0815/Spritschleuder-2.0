#include <stdio.h>
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "esp_system.h"
#include "esp_wifi.h"
#include "esp_event.h"
#include "esp_log.h"
#include "nvs_flash.h"

#include "config.h"
#include "cocktail_server.h"
#include "arduino_comm.h"
#include "display_manager.h"
#include "wifi_manager.h"
#include "button_manager.h"
#include "recipes.h"
#include "pump_settings.h"

static const char *TAG = "cocktailbot_main";

extern "C" void app_main(void)
{
    ESP_LOGI(TAG, "CocktailBot wird gestartet...");
    
    // NVS initialisieren
    esp_err_t ret = nvs_flash_init();
    if (ret == ESP_ERR_NVS_NO_FREE_PAGES || ret == ESP_ERR_NVS_NEW_VERSION_FOUND) {
        // Never destroy recipes/credentials to recover automatically.
        ESP_LOGE(TAG, "NVS muss gesichert und manuell repariert werden");
        return;
    }
    ESP_ERROR_CHECK(ret);
    ESP_ERROR_CHECK(RecipeStore::getInstance().init());
    ESP_ERROR_CHECK(PumpSettings::getInstance().init());
    
    // Display initialisieren
    DisplayManager::getInstance().init();
    DisplayManager::getInstance().showBootScreen();
    
    ESP_ERROR_CHECK(WifiManager::getInstance().init());
    
    // Arduino Kommunikation starten
    ArduinoComm::getInstance().init();
    ButtonManager::getInstance().init();
    
    // HTTP/WebSocket Server starten
    ESP_ERROR_CHECK(CocktailServer::getInstance().start());
    DisplayManager::getInstance().showReady();
    
    ESP_LOGI(TAG, "CocktailBot bereit!");
}
