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
         "bar_settings.cpp"
         "photo_store.cpp"
    INCLUDE_DIRS "."
    REQUIRES spiffs nvs_flash esp_netif esp_event esp_http_server esp_https_server mbedtls espressif__cjson esp_wifi esp_timer esp_driver_uart esp_driver_spi esp_driver_gpio
)

# Generate a deterministic gzip payload; the browser decodes it transparently.
# Keeping the editable HTML separate also lets the browser tests use it directly.
idf_build_get_property(sprit_python PYTHON)
set(sprit_web_gzip "${CMAKE_CURRENT_BINARY_DIR}/web.html.gz")
add_custom_command(OUTPUT "${sprit_web_gzip}"
    COMMAND "${sprit_python}" "${CMAKE_CURRENT_SOURCE_DIR}/../tools/embed_web.py"
            "${CMAKE_CURRENT_SOURCE_DIR}/web.html" "${sprit_web_gzip}"
    DEPENDS "${CMAKE_CURRENT_SOURCE_DIR}/web.html" "${CMAKE_CURRENT_SOURCE_DIR}/../tools/embed_web.py"
    VERBATIM)
target_add_binary_data(${COMPONENT_LIB} "${sprit_web_gzip}" BINARY)
