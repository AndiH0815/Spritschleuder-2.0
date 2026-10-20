// Build this separate target with tools/build_arduino.sh after verifying the
// board.
#include "../shared/pump_board.h"
#include <Arduino.h>
const uint8_t pumpPins[sprit::PumpCount] = {2, 3, 4, 5, 6, 7, 8, 9};
sprit::PumpBoard board;
sprit::Parser parser;
static void outputs() {
    for (size_t i = 0; i < sprit::PumpCount; ++i)
        digitalWrite(pumpPins[i], board.mask() & (1U << i) ? HIGH : LOW);
}
static void send(const sprit::Frame &f) {
    auto bytes = sprit::encode(f);
    Serial.write(bytes.data(), bytes.size());
}
void setup() {
    for (size_t i = 0; i < sprit::PumpCount; ++i) {
        digitalWrite(pumpPins[i], LOW);
        pinMode(pumpPins[i], OUTPUT);
    }
    Serial.begin(115200);
    sprit::Frame boot;
    boot.type = sprit::Message::Boot;
    send(boot);
}
void loop() {
    sprit::Frame f;
    // Bound RX work so noise or continuous serial traffic cannot starve timers.
    for (unsigned n = 0; n < 64 && Serial.available(); ++n)
        if (parser.feed(uint8_t(Serial.read()), millis(), f)) {
            auto response = board.handle(f, millis());
            outputs();
            send(response);
        }
    if (board.tick(millis(), f)) {
        outputs();
        send(f);
    }
}
