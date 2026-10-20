#include "../main/arduino_comm.h"
#include "../shared/pump_board.h"
#include <iostream>
uint32_t fake_ms = 0;
std::vector<uint8_t> fake_tx;
struct ControllerTest {
    static void exchange(ArduinoComm &esp, sprit::PumpBoard &board, bool drop = false) {
        auto tx = fake_tx;
        fake_tx.clear();
        sprit::Parser parser;
        sprit::Frame f;
        for (auto b : tx)
            if (parser.feed(b, fake_ms, f)) {
                auto reply = board.handle(f, fake_ms);
                if (!drop)
                    esp.receive(reply, fake_ms);
            }
    }
    static void advance(ArduinoComm &esp, sprit::PumpBoard &board, uint32_t ms,
                        const char *owner = "tablet", bool drop = false) {
        fake_ms = ms;
        if (owner)
            esp.heartbeat(owner, esp.current.job);
        sprit::Frame f;
        if (board.tick(ms, f) && !drop)
            esp.receive(f, ms);
        esp.poll(ms);
        exchange(esp, board, drop);
    }
    static void setup(ArduinoComm &esp, sprit::PumpBoard &board) {
        fake_ms = 0;
        fake_tx.clear();
        esp.init();
        fake_ms = 250;
        esp.poll(250);
        exchange(esp, board);
        assert(esp.status().online && !esp.busy());
    }
    static sprit::Plan plan() {
        sprit::Plan p;
        p.durations[0] = 1000;
        p.durations[7] = 2000;
        p.duration_ms = 2000;
        p.manual_ml = 340;
        return p;
    }
    static void run() {
        {
            ArduinoComm esp;
            sprit::PumpBoard board;
            setup(esp, board);
            std::string error;
            assert(esp.start(plan(), "tablet", error));
            assert(!esp.start(plan(), "other", error));
            exchange(esp, board);
            assert(board.mask() == 129);
            for (unsigned t = 500; t < 2250; t += 250)
                advance(esp, board, t);
            advance(esp, board, 2250);
            assert(!esp.busy() && esp.status().state == "done" && !board.mask());
        }
        {
            ArduinoComm esp;
            sprit::PumpBoard board;
            setup(esp, board);
            std::string e;
            esp.start(plan(), "tablet", e);
            exchange(esp, board);
            esp.abort();
            exchange(esp, board, true);
            assert(!board.mask() && esp.busy());
            for (unsigned t = 500; t < 3500; t += 250)
                advance(esp, board, t, nullptr, true);
            assert(esp.busy() && esp.status().state == "stopping");
            advance(esp, board, 3500, nullptr);
            assert(!esp.busy() && esp.status().state == "fault");
            assert(!esp.start(plan(), "tablet", e));
            esp.abort();
            exchange(esp, board);
            assert(esp.start(plan(), "tablet", e));
        }
        // A second browser cannot keep another tablet's output alive.
        {
            ArduinoComm esp;
            sprit::PumpBoard board;
            setup(esp, board);
            std::string e;
            esp.start(plan(), "tablet", e);
            exchange(esp, board);
            for (unsigned t = 500; t <= 1750; t += 250)
                advance(esp, board, t, "other");
            assert(!board.mask() && !esp.busy() && !esp.status().error.empty());
        }
        // UART loss: ESP stays locked; Arduino independently cuts power at
        // watchdog.
        {
            ArduinoComm esp;
            sprit::PumpBoard board;
            setup(esp, board);
            std::string e;
            esp.start(plan(), "tablet", e);
            exchange(esp, board);
            fake_tx.clear();
            for (unsigned t = 500; t <= 2000; t += 250) {
                fake_ms = t;
                esp.heartbeat("tablet", esp.current.job);
                esp.poll(t);
                fake_tx.clear();
                sprit::Frame f;
                board.tick(t, f);
            }
            assert(!board.mask() && esp.busy() && !esp.status().online);
        }
        // Reboot during output never resumes it; a fresh stop must be acknowledged.
        {
            ArduinoComm esp;
            sprit::PumpBoard board;
            setup(esp, board);
            std::string e;
            esp.start(plan(), "tablet", e);
            exchange(esp, board);
            board = sprit::PumpBoard();
            sprit::Frame boot;
            boot.type = sprit::Message::Boot;
            esp.receive(boot, 300);
            for (unsigned t = 500; t <= 1250; t += 250)
                advance(esp, board, t);
            assert(!board.mask() && !esp.busy() && esp.status().state == "fault");
        }
        std::cout << "ESP/Arduino integration: busy, confirmed completion, lost "
                     "stop ACK, owner loss, UART loss, reboot: PASS\n";
    }
};
int main() { ControllerTest::run(); }
