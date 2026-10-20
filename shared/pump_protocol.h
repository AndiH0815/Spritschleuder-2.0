#pragma once
#include <stddef.h>
#include <stdint.h>

// Protocol 2 is deliberately incompatible with the old 5-byte pump protocol.
// All integers are little endian; CRC-16/CCITT covers bytes 2..44.
namespace sprit {
// AVR Arduino does not ship libstdc++ containers; keep the wire layer
// freestanding.
template <typename T, size_t N> struct Array {
    T values[N]{};
    T *data() { return values; }
    const T *data() const { return values; }
    size_t size() const { return N; }
    T &operator[](size_t i) { return values[i]; }
    const T &operator[](size_t i) const { return values[i]; }
    T *begin() { return values; }
    T *end() { return values + N; }
    const T *begin() const { return values; }
    const T *end() const { return values + N; }
};
constexpr size_t PumpCount = 8;
constexpr uint32_t MaxRunMs = 300000;
constexpr uint32_t WatchdogMs = 1500;
constexpr size_t FrameSize = 47;
enum class Message : uint8_t {
    Hello = 1,
    Start,
    KeepAlive,
    Stop,
    Ready,
    Accepted,
    Status,
    Done,
    Stopped,
    Error,
    Boot
};
enum class Error : uint8_t { None, Invalid, Busy, Stale, Watchdog, Session };
struct Frame {
    Message type = Message::Hello;
    uint32_t job = 0, session = 0;
    Array<uint32_t, PumpCount> durations{};
    uint8_t code = 0; // response: actual output mask; Error: Error value
};
inline uint16_t crc16(const uint8_t *p, size_t n) {
    uint16_t crc = 0xffff;
    while (n--) {
        crc ^= uint16_t(*p++) << 8;
        for (int i = 0; i < 8; ++i)
            crc = (crc & 0x8000) ? uint16_t((crc << 1) ^ 0x1021) : uint16_t(crc << 1);
    }
    return crc;
}
inline void put32(uint8_t *p, uint32_t n) {
    for (int i = 0; i < 4; ++i)
        p[i] = uint8_t(n >> (8 * i));
}
inline uint32_t get32(const uint8_t *p) {
    uint32_t n = 0;
    for (int i = 0; i < 4; ++i)
        n |= uint32_t(p[i]) << (8 * i);
    return n;
}
inline Array<uint8_t, FrameSize> encode(const Frame &f) {
    Array<uint8_t, FrameSize> b{};
    b[0] = 0xa5;
    b[1] = 0x5a;
    b[2] = 2;
    b[3] = uint8_t(f.type);
    put32(b.data() + 4, f.job);
    put32(b.data() + 8, f.session);
    for (size_t i = 0; i < PumpCount; ++i)
        put32(b.data() + 12 + 4 * i, f.durations[i]);
    b[44] = f.code;
    auto crc = crc16(b.data() + 2, 43);
    b[45] = uint8_t(crc);
    b[46] = uint8_t(crc >> 8);
    return b;
}
class Parser {
    Array<uint8_t, FrameSize> bytes{};
    size_t used = 0;
    uint32_t last = 0;

  public:
    bool feed(uint8_t b, uint32_t now, Frame &f) {
        if (uint32_t(now - last) > 100)
            used = 0;
        last = now;
        if (!used && b != 0xa5)
            return false;
        bytes[used++] = b;
        if (used == 2 && b != 0x5a) {
            used = b == 0xa5 ? 1 : 0;
            return false;
        }
        if (used < FrameSize)
            return false;
        used = 0;
        if (bytes[2] != 2 || bytes[3] < 1 || bytes[3] > uint8_t(Message::Boot) ||
            crc16(bytes.data() + 2, 43) != (uint16_t(bytes[45]) | uint16_t(bytes[46]) << 8))
            return false;
        f.type = Message(bytes[3]);
        f.job = get32(bytes.data() + 4);
        f.session = get32(bytes.data() + 8);
        f.code = bytes[44];
        for (size_t i = 0; i < PumpCount; ++i)
            f.durations[i] = get32(bytes.data() + 12 + 4 * i);
        return true;
    }
};
} // namespace sprit
