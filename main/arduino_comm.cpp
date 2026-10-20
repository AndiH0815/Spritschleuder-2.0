#include "arduino_comm.h"
#ifndef SPRIT_HOST_TEST
#include "sdkconfig.h"
#include "config.h"
#include "display_manager.h"
#include "esp_log.h"
#include "esp_random.h"
#include "esp_timer.h"
#endif
namespace {
uint32_t nowMs() { return uint32_t(esp_timer_get_time() / 1000); }
} // namespace
ArduinoComm &ArduinoComm::getInstance() {
    static ArduinoComm i;
    return i;
}
void ArduinoComm::init() {
    uart_config_t c = {};
    c.baud_rate = UART_BAUD_RATE;
    c.data_bits = UART_DATA_8_BITS;
    c.parity = UART_PARITY_DISABLE;
    c.stop_bits = UART_STOP_BITS_1;
    c.flow_ctrl = UART_HW_FLOWCTRL_DISABLE;
    c.source_clk = UART_SCLK_DEFAULT;
    ESP_ERROR_CHECK(uart_driver_install(UART_PORT_NUM, 2048, 0, 0, nullptr, 0));
    ESP_ERROR_CHECK(uart_param_config(UART_PORT_NUM, &c));
    ESP_ERROR_CHECK(uart_set_pin(UART_PORT_NUM, UART_TX_PIN, UART_RX_PIN, UART_PIN_NO_CHANGE,
                                 UART_PIN_NO_CHANGE));
    session = esp_random();
    if (!session)
        session = 1;
    // Harmless with protocol 2; stop an old pump sketch too, but never START it.
    for (uint8_t p = 0; p < sprit::PumpCount; ++p) {
        const uint8_t off[] = {0xaa, p, 0, 0, 0x55};
        uart_write_bytes(UART_PORT_NUM, off, sizeof(off));
    }
    last_rx = nowMs();
    xTaskCreate(uart_task, "pump_link", 4096, this, 5, &task_handle);
}
void ArduinoComm::send(sprit::Message type) {
    sprit::Frame f;
    f.type = type;
    f.session = session;
    f.job = current.job;
    if (type == sprit::Message::Start)
        f.durations = plan.durations;
    auto bytes = sprit::encode(f);
    uart_write_bytes(UART_PORT_NUM, bytes.data(), bytes.size());
    last_tx = nowMs();
}
bool ArduinoComm::start(const sprit::Plan &p, const std::string &who, std::string &error) {
    std::lock_guard<std::mutex> lock(mutex);
#ifndef CONFIG_SPRIT_HARDWARE_CONFIRMED
    error = "Hardwarefreigabe fehlt: Board, Treiber und Pins zuerst pruefen";
    return false;
#endif
    if (current.busy) {
        error = "Eine Ausgabe laeuft bereits";
        return false;
    }
    if (!ready || !current.online) {
        error = "Pumpensteuerung nicht bereit (beide Firmwares aktualisieren)";
        return false;
    }
    if (!current.error.empty()) {
        error = "Fehler zuerst mit Stopp quittieren";
        return false;
    }
    if (!p.duration_ms || p.duration_ms > sprit::MaxRunMs || who.empty() ||
        next_job == UINT32_MAX) {
        error = "Auftrag ungueltig";
        return false;
    }
    for (auto ms : p.durations)
        if (ms > sprit::MaxRunMs) {
            error = "Laufzeit zu lang";
            return false;
        }
    plan = p;
    owner = who;
    started = owner_seen = nowMs();
    current = {};
    current.online = true;
    current.busy = true;
    current.state = "starting";
    current.job = ++next_job;
    current.duration_ms = p.duration_ms;
    current.manual_ml = p.manual_ml;
    send(sprit::Message::Start);
    return true;
}
void ArduinoComm::heartbeat(const std::string &who, uint32_t job) {
    std::lock_guard<std::mutex> lock(mutex);
    if (current.busy && !stopping && who == owner && job == current.job)
        owner_seen = nowMs();
}
void ArduinoComm::abort() {
    std::lock_guard<std::mutex> lock(mutex);
    if (!current.busy)
        current.error.clear();
    if (!stopping) {
        stopping_at = nowMs();
        stopping = true;
    }
    current.busy = true;
    current.state = "stopping";
    send(sprit::Message::Stop);
}
ControllerStatus ArduinoComm::status() {
    std::lock_guard<std::mutex> lock(mutex);
    auto s = current;
    if (s.busy)
        s.elapsed_ms = nowMs() - started;
    return s;
}
bool ArduinoComm::busy() {
    std::lock_guard<std::mutex> lock(mutex);
    return current.busy;
}
void ArduinoComm::receive(const sprit::Frame &f, uint32_t now) {
    if (f.type == sprit::Message::Boot) {
        if (current.busy) {
            current.error = "Pumpencontroller neu gestartet";
            stopping = true;
            stopping_at = now;
            current.state = "stopping";
        }
        ready = false;
        return;
    }
    if (f.session != session)
        return;
    last_rx = now;
    current.online = true;
    if (f.type == sprit::Message::Ready && f.code == 0) {
        ready = true;
        if (!current.busy)
            current.state = current.error.empty() ? "idle" : "fault";
        return;
    }
    if (f.type == sprit::Message::Stopped && stopping && f.code == 0) {
        current.outputs = 0;
        current.job = f.job;
        current.busy = false;
        stopping = false;
        ready = true;
        current.state = current.error.empty() ? "aborted" : "fault";
        DisplayManager::getInstance().showAborted();
        return;
    }
    if (f.job != current.job)
        return;
    if (f.type == sprit::Message::Error) {
        current.error = "Fehler der Pumpensteuerung (" + std::to_string(f.code) + ")";
        if (!stopping) {
            stopping = true;
            stopping_at = now;
        }
        current.state = "stopping";
        current.busy = true;
        send(sprit::Message::Stop);
        return;
    }
    if (stopping)
        return;
    if (f.type == sprit::Message::Stopped && current.busy) {
        current.error = "Pumpen unerwartet gestoppt";
        stopping = true;
        stopping_at = now;
        current.state = "stopping";
        send(sprit::Message::Stop);
        return;
    }
    if (f.type == sprit::Message::Accepted || f.type == sprit::Message::Status) {
        current.outputs = f.code;
        if (current.busy)
            current.state = "running";
        else if (f.code) {
            current.error = "Unerwartet aktive Pumpen";
            stopping = true;
            stopping_at = now;
            current.busy = true;
            current.state = "stopping";
            send(sprit::Message::Stop);
        }
    } else if (f.type == sprit::Message::Done && current.busy && f.code == 0) {
        current.outputs = 0;
        current.busy = false;
        current.state = "done";
        DisplayManager::getInstance().showReady();
    }
}
void ArduinoComm::uart_task(void *arg) {
    auto *c = static_cast<ArduinoComm *>(arg);
    uint8_t buf[128];
    for (;;) {
        int n = uart_read_bytes(UART_PORT_NUM, buf, sizeof(buf), pdMS_TO_TICKS(20));
        std::lock_guard<std::mutex> lock(c->mutex);
        const uint32_t now = nowMs();
        sprit::Frame f;
        for (int i = 0; i < n; ++i)
            if (c->parser.feed(buf[i], now, f))
                c->receive(f, now);
        c->poll(now);
    }
}
void ArduinoComm::poll(uint32_t now) {
    if (uint32_t(now - last_rx) >= sprit::WatchdogMs) {
        current.online = false;
        ready = false;
        if (current.busy && !stopping) {
            current.error = "Verbindung zum Pumpencontroller verloren";
            stopping = true;
            stopping_at = now;
            current.state = "stopping";
        }
    }
    if (current.busy && !stopping &&
        (uint32_t(now - owner_seen) >= sprit::WatchdogMs ||
         uint32_t(now - started) > plan.duration_ms + 2000)) {
        current.error = uint32_t(now - owner_seen) >= sprit::WatchdogMs
                            ? "Bedientablet nicht erreichbar"
                            : "Fertigmeldung fehlt";
        stopping = true;
        stopping_at = now;
        current.state = "stopping";
    }
    if (stopping) {
        if (uint32_t(now - last_tx) >= 250) {
            send(sprit::Message::Stop);
            if (!ready)
                send(sprit::Message::Hello);
        }
        // Stay locked until outputs OFF are confirmed; no timer-based success.
        if (uint32_t(now - stopping_at) > 2000 && current.error.empty())
            current.error = "Stopp noch nicht bestaetigt";
        // A rebooted board has no session: stop first, then establish ours.

    } else if (uint32_t(now - last_tx) >= 250) {
        send(ready ? sprit::Message::KeepAlive : sprit::Message::Hello);
    }
}
