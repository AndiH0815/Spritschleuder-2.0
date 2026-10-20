#pragma once
#include <cassert>
#include <cstddef>
#include <cstdint>
#include <vector>
using TaskHandle_t = void *;
extern uint32_t fake_ms;
extern std::vector<uint8_t> fake_tx;
inline int64_t esp_timer_get_time() { return int64_t(fake_ms) * 1000; }
inline uint32_t esp_random() { return 42; }
constexpr int UART_PORT_NUM = 1, UART_BAUD_RATE = 115200, UART_TX_PIN = 6, UART_RX_PIN = 7;
constexpr int UART_DATA_8_BITS = 0, UART_PARITY_DISABLE = 0, UART_STOP_BITS_1 = 0,
              UART_HW_FLOWCTRL_DISABLE = 0, UART_SCLK_DEFAULT = 0, UART_PIN_NO_CHANGE = -1;
struct uart_config_t {
    int baud_rate, data_bits, parity, stop_bits, flow_ctrl, source_clk;
};
inline int uart_driver_install(int, int, int, int, void *, int) { return 0; }
inline int uart_param_config(int, uart_config_t *) { return 0; }
inline int uart_set_pin(int, int, int, int, int) { return 0; }
inline int uart_write_bytes(int, const void *p, size_t n) {
    auto b = static_cast<const uint8_t *>(p);
    fake_tx.insert(fake_tx.end(), b, b + n);
    return int(n);
}
inline int uart_read_bytes(int, uint8_t *, size_t, int) { return 0; }
inline void xTaskCreate(void (*)(void *), const char *, int, void *, int, TaskHandle_t *) {}
#define ESP_ERROR_CHECK(e) assert((e) == 0)
#define pdMS_TO_TICKS(ms) (ms)
struct DisplayManager {
    static DisplayManager &getInstance() {
        static DisplayManager i;
        return i;
    }
    void showReady() {}
    void showAborted() {}
};
