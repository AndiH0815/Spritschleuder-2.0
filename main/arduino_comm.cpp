#include "arduino_comm.h"
#include "config.h"
#include "esp_log.h"
#include "recipes.h"
#include <algorithm>

static const char *TAG = "arduino_comm";

ArduinoComm& ArduinoComm::getInstance() {
    static ArduinoComm instance;
    return instance;
}

void ArduinoComm::init() {
    uart_config_t uart_config = {};
    uart_config.baud_rate = UART_BAUD_RATE;
    uart_config.data_bits = UART_DATA_8_BITS;
    uart_config.parity = UART_PARITY_DISABLE;
    uart_config.stop_bits = UART_STOP_BITS_1;
    uart_config.flow_ctrl = UART_HW_FLOWCTRL_DISABLE;
    uart_config.source_clk = UART_SCLK_DEFAULT;
    
    uart_driver_install(UART_PORT_NUM, 2048, 0, 0, nullptr, 0);
    uart_param_config(UART_PORT_NUM, &uart_config);
    uart_set_pin(UART_PORT_NUM, UART_TX_PIN, UART_RX_PIN, UART_PIN_NO_CHANGE, UART_PIN_NO_CHANGE);
    
    xTaskCreate(uart_task, "uart_task", 4096, this, 5, &uart_task_handle);
    ESP_LOGI(TAG, "UART Kommunikation initialisiert");
}

void ArduinoComm::sendOrder(const CocktailOrder& order) {
    std::lock_guard<std::mutex> lock(queue_mutex);
    const Recipe &recipe = order.recipe;
    uint32_t total = 0;
    for (uint8_t i = 0; i < recipe.num_ingredients; ++i) total += recipe.ingredients[i].amount_ml;
    for (uint8_t i = 0; i < recipe.num_ingredients; ++i) {
        uint32_t ml = recipe.ingredients[i].amount_ml * order.quantity_ml / total;
        // Kalibrierwert der Altanlage: 200 ml in 90 s => 450 ms/ml.
        uint32_t duration = std::min<uint32_t>(ml * 450U, 65535U);
        command_queue.push(ArduinoCommand{recipe.ingredients[i].pump_id, static_cast<uint16_t>(duration)});
    }
}

void ArduinoComm::abort() {
    {
        std::lock_guard<std::mutex> lock(queue_mutex);
        ++abort_generation;
        std::queue<ArduinoCommand> empty; command_queue.swap(empty);
    }
    // Dauer 0 ist ein sofortiges AUS und funktioniert fuer jede Firmware-Version.
    std::lock_guard<std::mutex> lock(uart_mutex);
    for (uint8_t pump = 0; pump < 4; ++pump) {
        const uint8_t frame[5] = {0xAA, pump, 0, 0, 0x55};
        uart_write_bytes(UART_PORT_NUM, frame, sizeof(frame));
    }
    uart_wait_tx_done(UART_PORT_NUM, pdMS_TO_TICKS(50));
    ESP_LOGW(TAG, "NOT-AUS: alle Pumpen gestoppt");
}

void ArduinoComm::uart_task(void *param) {
    auto *comm = static_cast<ArduinoComm*>(param);
    
    while (true) {
        ArduinoCommand cmd;
        uint32_t generation;
        
        {
            std::lock_guard<std::mutex> lock(comm->queue_mutex);
            if (comm->command_queue.empty()) {
                vTaskDelay(10 / portTICK_PERIOD_MS);
                continue;
            }
            cmd = comm->command_queue.front();
            comm->command_queue.pop();
            generation = comm->abort_generation.load();
        }
        
        // Protokoll: [START][PUMP_ID][DURATION_HI][DURATION_LO][END]
        uint8_t buffer[5] = {
            0xAA,                    // Start byte
            cmd.pump_id,             // Pumpen ID
            (uint8_t)(cmd.duration_ms >> 8),  // Dauer High byte
            (uint8_t)(cmd.duration_ms & 0xFF), // Dauer Low byte
            0x55                     // End byte
        };
        
        {
            std::lock_guard<std::mutex> lock(comm->uart_mutex);
            // Falls NOT-AUS zwischen Entnahme und UART-Zugriff lag, darf dieser
            // bereits entnommene Befehl die Pumpe keinesfalls erneut starten.
            if (generation != comm->abort_generation.load()) continue;
            uart_write_bytes(UART_PORT_NUM, buffer, sizeof(buffer));
        }
        ESP_LOGI(TAG, "Befehl gesendet: Pumpe %d, %d ms", cmd.pump_id, cmd.duration_ms);
        
        // Alle Startbefehle eines Rezeptes direkt hintereinander senden. Der Arduino
        // schaltet die Pumpen unabhaengig und damit gleichzeitig.
        taskYIELD();
    }
}
