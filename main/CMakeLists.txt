idf_component_register(
    SRCS "main.cpp" 
         "cocktail_server.cpp" 
         "arduino_comm.cpp"
         "wifi_manager.cpp"
         "display_manager.cpp"
         "recipes.cpp"
         "button_manager.cpp"
         "pump_settings.cpp"
         "access_control.cpp"
    INCLUDE_DIRS "."
    EMBED_FILES "web.html"
    REQUIRES nvs_flash esp_netif esp_event esp_http_server esp_https_server mbedtls espressif__cjson esp_wifi esp_timer esp_driver_uart esp_driver_spi esp_driver_gpio
)
