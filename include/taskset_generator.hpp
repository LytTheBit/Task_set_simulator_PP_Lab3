// taskset_generator.hpp
// Created by Francesco on 17/02/2026.
//
// Generatore di task set periodici con utilizzo target.
// - periodi uniformi in [Tmin, Tmax]
// - deadline = period (implicit deadline)
// - WCET calcolato per raggiungere utilizzo target
// - priorità assegnata secondo Rate Monotonic

#pragma once

#include <vector>
#include <random>
#include <algorithm>
#include <stdexcept>

#include "task.hpp"

namespace rt {

struct GeneratorConfig {
    std::int32_t n_tasks = 5;
    tick_t Tmin = 10;
    tick_t Tmax = 100;
    double utilization_target = 0.75;
    std::uint32_t seed = 1;
};

class TaskSetGenerator {
public:
    static std::vector<Task> generate(const GeneratorConfig& cfg) {
        if (cfg.n_tasks <= 0)
            throw std::invalid_argument("n_tasks must be > 0");

        std::mt19937 rng(cfg.seed);
        std::uniform_int_distribution<tick_t> period_dist(cfg.Tmin, cfg.Tmax);

        std::vector<tick_t> periods(cfg.n_tasks);
        for (int i = 0; i < cfg.n_tasks; ++i) {
            periods[i] = period_dist(rng);
        }

        // Distribuzione uniforme semplice delle frazioni di utilizzo
        std::uniform_real_distribution<double> u_dist(0.0, 1.0);

        std::vector<double> u(cfg.n_tasks);
        double sum_u = 0.0;
        for (int i = 0; i < cfg.n_tasks; ++i) {
            u[i] = u_dist(rng);
            sum_u += u[i];
        }

        // Normalizza per ottenere somma = utilization_target
        for (int i = 0; i < cfg.n_tasks; ++i) {
            u[i] = (u[i] / sum_u) * cfg.utilization_target;
        }

        std::vector<Task> tasks;
        tasks.reserve(cfg.n_tasks);

        for (int i = 0; i < cfg.n_tasks; ++i) {
            tick_t T = periods[i];
            tick_t C = static_cast<tick_t>(u[i] * T);

            if (C < 1) C = 1;
            if (C >= T) C = T - 1;

            Task t;
            t.id = i;
            t.period = T;
            t.deadline = T;
            t.wcet = C;
            t.offset = 0;

            tasks.push_back(t);
        }

        // Assegna priorità RM: periodo minore → priorità maggiore (numero più piccolo)
        std::vector<int> indices(cfg.n_tasks);
        for (int i = 0; i < cfg.n_tasks; ++i) indices[i] = i;

        std::sort(indices.begin(), indices.end(),
                  [&](int a, int b) {
                      return tasks[a].period < tasks[b].period;
                  });

        for (int prio = 0; prio < cfg.n_tasks; ++prio) {
            tasks[indices[prio]].priority = prio;
        }

        return tasks;
    }
};

} // namespace rt