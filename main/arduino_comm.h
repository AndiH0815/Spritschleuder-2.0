#pragma once
#include "../shared/dosing.h"
#ifdef SPRIT_HOST_TEST
#include "../tests/fakes/platform.h"
#else
#include "driver/uart.h"
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#endif
#include <mutex>
#include <string>

struct ControllerStatus {
    std::string state = "offline", error;
    bool online = false, busy = false;
    uint32_t job = 0, elapsed_ms = 0, duration_ms = 0;
    uint8_t outputs = 0;
    double manual_ml = 0;
};
class ArduinoComm {
  public:
    static ArduinoComm &getInstance();
    void init();
    bool start(const sprit::Plan &, const std::string &owner, std::string &error);
    void heartbeat(const std::string &owner, uint32_t job);
    void abort();
    ControllerStatus status();
    bool busy();

  private:
    friend struct ControllerTest;
    ArduinoComm() = default;
    void poll(uint32_t now);
    static void uart_task(void *);
    void send(sprit::Message type);
    void receive(const sprit::Frame &, uint32_t now);
    std::mutex mutex;
    ControllerStatus current;
    sprit::Plan plan;
    sprit::Parser parser;
    std::string owner;
    uint32_t session = 0, next_job = 0, started = 0, last_rx = 0, last_tx = 0, owner_seen = 0,
             stopping_at = 0;
    bool ready = false, stopping = false;
    TaskHandle_t task_handle = nullptr;
};
