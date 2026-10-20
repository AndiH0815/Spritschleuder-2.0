#pragma once
#include "pump_protocol.h"

namespace sprit {
// Hardware-independent output state. The Arduino adapter copies mask to its
// pins.
class PumpBoard {
    uint32_t session_ = 0, job_ = 0, highest_ = 0, started_ = 0, heartbeat_ = 0;
    Array<uint32_t, PumpCount> durations_{};
    uint8_t mask_ = 0;
    Message terminal_ = Message::Done;
    Frame reply(Message type, Error error = Error::None) const {
        Frame f;
        f.type = type;
        f.session = session_;
        f.job = job_;
        f.code = type == Message::Error ? uint8_t(error) : mask_;
        return f;
    }

  public:
    uint8_t mask() const { return mask_; }
    Frame handle(const Frame &f, uint32_t now) {
        if (f.type == Message::Stop) { // stop ALWAYS acts, including wrong session
            mask_ = 0;
            terminal_ = Message::Stopped;
            return reply(Message::Stopped);
        }
        if (f.type == Message::Hello) {
            if (!f.session)
                return reply(Message::Error, Error::Invalid);
            // Repeated Hello must never reset the high-water mark in one session.
            if (f.session != session_) {
                mask_ = 0;
                session_ = f.session;
                job_ = highest_ = 0;
                terminal_ = Message::Stopped;
            }
            return reply(mask_ ? Message::Status : Message::Ready);
        }
        if (!session_ || f.session != session_)
            return reply(Message::Error, Error::Session);
        if (f.type == Message::KeepAlive) {
            if (f.job != job_)
                return reply(Message::Error, Error::Stale);
            heartbeat_ = now;
            return reply(mask_ ? Message::Status : terminal_);
        }
        if (f.type != Message::Start)
            return reply(Message::Error, Error::Invalid);
        // Retry an accepted or finished job without ever restarting outputs.
        if (f.job && f.job == job_)
            return reply(mask_ ? Message::Accepted : terminal_);
        if (!f.job || f.job <= highest_)
            return reply(Message::Error, Error::Stale);
        if (mask_)
            return reply(Message::Error, Error::Busy);
        uint8_t next = 0;
        for (size_t i = 0; i < PumpCount; ++i) {
            if (f.durations[i] > MaxRunMs)
                return reply(Message::Error, Error::Invalid);
            if (f.durations[i])
                next |= uint8_t(1U << i);
        }
        if (!next)
            return reply(Message::Error, Error::Invalid);
        job_ = highest_ = f.job;
        durations_ = f.durations;
        started_ = heartbeat_ = now;
        mask_ = next;
        terminal_ = Message::Done;
        return reply(Message::Accepted);
    }
    bool tick(uint32_t now, Frame &f) {
        if (!mask_)
            return false;
        if (uint32_t(now - heartbeat_) >= WatchdogMs) {
            mask_ = 0;
            terminal_ = Message::Stopped;
            f = reply(Message::Error, Error::Watchdog);
            return true;
        }
        const uint8_t before = mask_;
        for (size_t i = 0; i < PumpCount; ++i)
            if ((mask_ & (1U << i)) && uint32_t(now - started_) >= durations_[i])
                mask_ &= uint8_t(~(1U << i));
        if (before == mask_)
            return false;
        f = reply(mask_ ? Message::Status : Message::Done);
        return true;
    }
};
} // namespace sprit
