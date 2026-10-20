#pragma once
#include "pump_protocol.h"
#include <array>
#include <cmath>

namespace sprit {
struct Pump {
    bool active;
    uint16_t ingredient;
    double ml_per_second;
    Pump(bool a = false, uint16_t i = 0, double r = 0)
        : active(a), ingredient(i), ml_per_second(r) {}
};
struct Portion {
    uint16_t ingredient;
    double ml;
    bool strength;
    Portion(uint16_t i = 0, double m = 0, bool s = false) : ingredient(i), ml(m), strength(s) {}
};
struct DosingRecipe {
    std::array<Portion, PumpCount> portions{};
    size_t count = 0;
    double base_ml = 400;
};
struct Plan {
    Array<uint32_t, PumpCount> durations{};
    double automatic_ml = 0, manual_ml = 0;
    uint32_t duration_ms = 0;
};
enum class PlanError { None, Invalid, Unavailable, Uncalibrated, TooLong, Overflow };
inline PlanError calculate(const DosingRecipe &r, const std::array<Pump, PumpCount> &pumps,
                           double target, double strength, Plan &plan) {
    plan = {};
    if (r.count == 0 || r.count > PumpCount || r.base_ml != 400 || !std::isfinite(target) ||
        target < 20 || target > 1000 || (strength != 0.5 && strength != 1 && strength != 1.5))
        return PlanError::Invalid;
    for (size_t i = 0; i < PumpCount; ++i)
        if (pumps[i].active) {
            if (!pumps[i].ingredient)
                return PlanError::Invalid;
            for (size_t j = 0; j < i; ++j)
                if (pumps[j].active && pumps[j].ingredient == pumps[i].ingredient)
                    return PlanError::Invalid;
        }
    double base_total = 0;
    for (size_t i = 0; i < r.count; ++i) {
        const auto &p = r.portions[i];
        if (!p.ingredient || !std::isfinite(p.ml) || p.ml <= 0 || p.ml > 400)
            return PlanError::Invalid;
        for (size_t j = 0; j < i; ++j)
            if (r.portions[j].ingredient == p.ingredient)
                return PlanError::Invalid;
        base_total += p.ml;
        size_t slot = PumpCount;
        for (size_t j = 0; j < PumpCount; ++j)
            if (pumps[j].active && pumps[j].ingredient == p.ingredient)
                slot = j;
        if (slot == PumpCount)
            return PlanError::Unavailable;
        const double rate = pumps[slot].ml_per_second;
        if (!std::isfinite(rate) || rate < 0.01 || rate > 100)
            return PlanError::Uncalibrated;
        double ml = p.ml * target / r.base_ml * (p.strength ? strength : 1.0);
        double ms = std::ceil(ml / rate * 1000.0);
        if (ms > MaxRunMs)
            return PlanError::TooLong;
        plan.durations[slot] = uint32_t(ms);
        plan.automatic_ml += ml;
        if (plan.durations[slot] > plan.duration_ms)
            plan.duration_ms = plan.durations[slot];
    }
    if (base_total > 400 || plan.automatic_ml > target)
        return PlanError::Overflow;
    plan.manual_ml = target - plan.automatic_ml;
    return PlanError::None;
}
} // namespace sprit
