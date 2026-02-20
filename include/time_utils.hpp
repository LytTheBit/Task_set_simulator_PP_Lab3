// time_utils.hpp
// Created by Francesco on 17/02/2026.
//
// Utility per operazioni su tick/periodi: gcd, lcm, iperperiodo (LCM dei periodi).
// Include protezione base da overflow.

#pragma once

#include <cstdint>
#include <limits>
#include <stdexcept>
#include <vector>

#include "task.hpp"

namespace rt {

    inline tick_t gcd_tick(tick_t a, tick_t b) {
        if (a < 0) a = -a;
        if (b < 0) b = -b;
        while (b != 0) {
            tick_t t = a % b;
            a = b;
            b = t;
        }
        return a;
    }

    inline tick_t lcm_tick(tick_t a, tick_t b) {
        if (a == 0 || b == 0) return 0;
        tick_t g = gcd_tick(a, b);

        // a/g * b, controllando overflow
        tick_t a_div_g = a / g;
        if (a_div_g > std::numeric_limits<tick_t>::max() / b) {
            throw std::overflow_error("LCM overflow");
        }
        return a_div_g * b;
    }

    // Iperperiodo = LCM dei periodi. Se overflow, lancia eccezione.
    inline tick_t hyperperiod(const std::vector<Task>& tasks) {
        if (tasks.empty()) return 0;
        tick_t hp = tasks[0].period;
        for (size_t i = 1; i < tasks.size(); ++i) {
            hp = lcm_tick(hp, tasks[i].period);
        }
        return hp;
    }

} // namespace rt