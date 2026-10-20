#include "../shared/dosing.h"
#include "../shared/pump_board.h"
#include <cassert>
#include <iostream>
#include <limits>
using namespace sprit;
static Frame hello(uint32_t session = 42) {
    Frame f;
    f.type = Message::Hello;
    f.session = session;
    return f;
}
static Frame start(uint32_t id = 1, uint32_t ms = 1000) {
    Frame f;
    f.type = Message::Start;
    f.session = 42;
    f.job = id;
    f.durations[0] = ms;
    return f;
}
void protocol() {
    Frame f = start();
    f.durations[7] = 123456;
    auto b = encode(f);
    Parser p;
    Frame out;
    bool got = false;
    for (auto byte : b) {
        got = p.feed(byte, 0, out);
    }
    assert(got && out.job == 1 && out.durations[7] == 123456);
    for (size_t i = 2; i < b.size(); ++i) {
        auto corrupt = b;
        corrupt[i] ^= 1;
        Parser q;
        bool accepted = false;
        for (auto byte : corrupt)
            accepted |= q.feed(byte, 0, out);
        assert(!accepted);
    }
    Parser fragmented;
    for (size_t i = 0; i < 20; ++i)
        assert(!fragmented.feed(b[i], 0, out));
    bool accepted = false;
    for (size_t i = 20; i < b.size(); ++i)
        accepted |= fragmented.feed(b[i], 101, out);
    assert(!accepted);
    Parser sync;
    sync.feed(0x13, 0, out);
    sync.feed(0xa5, 0, out);
    for (auto byte : b)
        got = sync.feed(byte, 0, out);
    assert(got);
}
void output_cases() {
    PumpBoard board;
    Frame out;
    assert(board.mask() == 0);
    assert(board.handle(start(), 0).type == Message::Error);
    board.handle(hello(), 0);
    Frame f = start();
    f.durations[7] = 2000;
    assert(board.handle(f, 0).type == Message::Accepted && board.mask() == 129);
    assert(board.handle(start(2), 1).type == Message::Error);
    assert(board.handle(f, 500).type == Message::Accepted);
    // Duplicate Start must not extend either timer. Independent output 8 remains
    // on.
    board.tick(1000, out);
    assert(board.mask() == 128);
    Frame heartbeat = f;
    heartbeat.type = Message::KeepAlive;
    board.handle(heartbeat, 1000);
    board.tick(2000, out);
    assert(board.mask() == 0 && out.type == Message::Done);
    assert(board.handle(f, 2001).type == Message::Done && !board.mask());
    // Repeated handshake in same session cannot erase job high-water mark.
    board.handle(hello(), 2010);
    assert(board.handle(f, 2011).type == Message::Done && !board.mask());
    f = start(2, MaxRunMs);
    f.durations[7] = MaxRunMs;
    board.handle(f, 3000);
    board.tick(4499, out);
    assert(board.mask());
    assert(board.tick(4500, out) && !board.mask() && out.code == uint8_t(Error::Watchdog));
    assert(board.handle(f, 4501).type == Message::Stopped && !board.mask());
    Frame stop;
    stop.type = Message::Stop;
    stop.session = 999;
    board.handle(stop, 4502);
    assert(!board.mask());
    f = start(3);
    for (auto &d : f.durations)
        d = 1000;
    board.handle(f, 5000);
    assert(board.mask() == 255);
    board.handle(stop, 5001);
    assert(!board.mask());
    assert(board.handle(start(1), 5100).type == Message::Error);
    auto tooLong = start(4, MaxRunMs + 1);
    assert(board.handle(tooLong, 5200).type == Message::Error && !board.mask());
    // millis rollover: an output is not confused with an unset deadline.
    PumpBoard wrap;
    wrap.handle(hello(), UINT32_MAX - 500);
    wrap.handle(start(), UINT32_MAX - 500);
    wrap.tick(499, out);
    assert(!wrap.mask() && out.type == Message::Done);
    // Old session frames cannot move outputs after ESP reset/handshake.
    wrap.handle(hello(43), 500);
    assert(wrap.handle(start(2), 501).type == Message::Error && !wrap.mask());
}
void dose_cases() {
    DosingRecipe r;
    r.count = 2;
    r.portions[0] = {1, 40, true};
    r.portions[1] = {4, 20, false};
    std::array<Pump, 8> p{};
    p[0] = {true, 1, 2};
    p[3] = {true, 4, 1};
    Plan plan;
    assert(calculate(r, p, 400, 1, plan) == PlanError::None && plan.manual_ml == 340 &&
           plan.durations[0] == 20000 && plan.durations[3] == 20000);
    calculate(r, p, 200, 1, plan);
    assert(plan.manual_ml == 170 && plan.automatic_ml == 30);
    calculate(r, p, 400, 0.5, plan);
    assert(plan.manual_ml == 360 && plan.durations[0] == 10000);
    calculate(r, p, 400, 1.5, plan);
    assert(plan.manual_ml == 320 && plan.durations[0] == 30000);
    // Move stable ingredient identity to channel 8 without editing the recipe.
    p[7] = p[0];
    p[0].active = false;
    assert(calculate(r, p, 400, 1, plan) == PlanError::None && plan.durations[0] == 0 &&
           plan.durations[7] == 20000);
    p[7].active = false;
    assert(calculate(r, p, 400, 1, plan) == PlanError::Unavailable);
    p[7].active = true;
    p[7].ml_per_second = 0;
    assert(calculate(r, p, 400, 1, plan) == PlanError::Uncalibrated);
    p[7].ml_per_second = 0.01;
    assert(calculate(r, p, 400, 1, plan) == PlanError::TooLong);
    p[7].ml_per_second = 2;
    p[0].active = true;
    assert(calculate(r, p, 400, 1, plan) == PlanError::Invalid);
    p[0].active = false;
    assert(calculate(r, p, std::numeric_limits<double>::quiet_NaN(), 1, plan) ==
           PlanError::Invalid);
    assert(calculate(r, p, 400, 1.25, plan) == PlanError::Invalid);
    r.portions[0].ml = 300;
    assert(calculate(r, p, 400, 1.5, plan) == PlanError::Overflow);
    r.portions[0].ml = 40;
    p[7].ml_per_second = 0.2;
    assert(calculate(r, p, 400, 1, plan) == PlanError::None &&
           plan.durations[7] == 200000); // no 65535ms clamp
}
int main() {
    protocol();
    output_cases();
    dose_cases();
    std::cout << "protocol, independent outputs, replay, watchdog, stop, "
                 "rollover and dosing: PASS\n";
}
