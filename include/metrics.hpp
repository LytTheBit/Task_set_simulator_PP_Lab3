// metrics.hpp
// Created by Francesco on 17/02/2026.
//
// Strutture per la raccolta di metriche di simulazione:
// - metriche globali (utilization, deadline miss, unfinished jobs)
// - metriche per task (numero job, response time medio/max, lateness, unfinished)

#pragma once

#include <vector>
#include <cstdint>
#include <iostream>
#include <iomanip>
#include <string>

#include "task.hpp"
#include "job.hpp"

namespace rt {

struct TaskMetrics {
    id_t task_id = 0;

    std::int64_t jobs_released = 0;
    std::int64_t jobs_completed = 0;

    // Deadline miss: conteggiato quando un job completa con finish_time > abs_deadline.
    std::int64_t deadline_miss = 0;

    // Job rilasciati ma non completati entro l'orizzonte di simulazione.
    std::int64_t unfinished = 0;

    // Response time (finish - release)
    tick_t rt_sum = 0;
    tick_t rt_max = 0;

    // Lateness = max(0, finish - abs_deadline)
    tick_t lateness_sum = 0;
    tick_t lateness_max = 0;

    void on_job_released() {
        jobs_released++;
    }

    void on_job_completed(const Job& j) {
        jobs_completed++;

        if (auto rt = j.response_time(); rt.has_value()) {
            rt_sum += *rt;
            if (*rt > rt_max) rt_max = *rt;
        }

        if (j.finish_time.has_value()) {
            tick_t late = *j.finish_time - j.abs_deadline;
            if (late > 0) {
                lateness_sum += late;
                if (late > lateness_max) lateness_max = late;
                deadline_miss++;
            }
        }
    }

    void finalize_unfinished() {
        unfinished = jobs_released - jobs_completed;
    }

    double avg_response_time() const {
        if (jobs_completed == 0) return 0.0;
        return static_cast<double>(rt_sum) / static_cast<double>(jobs_completed);
    }

    double avg_lateness() const {
        if (jobs_completed == 0) return 0.0;
        return static_cast<double>(lateness_sum) / static_cast<double>(jobs_completed);
    }
};

struct SimulationMetrics {
    tick_t horizon = 0;
    tick_t busy_ticks = 0;

    std::int64_t deadline_miss_total = 0;
    std::int64_t unfinished_total = 0;

    std::vector<TaskMetrics> per_task;

    double utilization() const {
        return horizon > 0 ? static_cast<double>(busy_ticks) / static_cast<double>(horizon) : 0.0;
    }

    void init_from_tasks(const std::vector<Task>& tasks, tick_t horizon_ticks) {
        horizon = horizon_ticks;
        busy_ticks = 0;
        deadline_miss_total = 0;
        unfinished_total = 0;

        per_task.clear();
        per_task.reserve(tasks.size());
        for (const auto& t : tasks) {
            TaskMetrics tm;
            tm.task_id = t.id;
            per_task.push_back(tm);
        }
    }

    // Chiamare alla fine della simulazione per calcolare metriche aggregate:
    // - deadline_miss_total
    // - unfinished_total
    void finalize() {
        deadline_miss_total = 0;
        unfinished_total = 0;

        for (auto& tm : per_task) {
            tm.finalize_unfinished();
            deadline_miss_total += tm.deadline_miss;
            unfinished_total += tm.unfinished;
        }
    }

    // Output leggibile (tabellare)
    void print_summary(std::ostream& os, const std::vector<Task>& tasks) const {
        os << "\n=== Simulation results ===\n";
        os << "Horizon (ticks): " << horizon << "   (1 tick = 1 ms)\n";
        os << "CPU busy ticks:  " << busy_ticks << "\n";
        os << "CPU utilization: " << std::fixed << std::setprecision(6) << utilization() << "\n";
        os << "Deadline miss:   " << deadline_miss_total << "\n";
        os << "Unfinished jobs: " << unfinished_total
           << "  (released but not completed within horizon)\n";

        os << "\nPer-task metrics:\n";
        os << std::left
           << std::setw(6)  << "ID"
           << std::setw(6)  << "Prio"
           << std::setw(6)  << "T"
           << std::setw(6)  << "D"
           << std::setw(6)  << "C"
           << std::setw(10) << "Rel"
           << std::setw(10) << "Comp"
           << std::setw(10) << "Miss"
           << std::setw(12) << "Unfinished"
           << std::setw(12) << "RT_avg"
           << std::setw(10) << "RT_max"
           << std::setw(12) << "Late_avg"
           << std::setw(10) << "Late_max"
           << "\n";

        os << std::string(6+6+6+6+6+10+10+10+12+12+10+12+10, '-') << "\n";

        // Assumiamo che per_task sia ordinato nello stesso ordine di tasks (task_index).
        for (size_t i = 0; i < tasks.size(); ++i) {
            const auto& t = tasks[i];
            const auto& m = per_task[i];

            os << std::left
               << std::setw(6)  << t.id
               << std::setw(6)  << t.priority
               << std::setw(6)  << t.period
               << std::setw(6)  << t.deadline
               << std::setw(6)  << t.wcet
               << std::setw(10) << m.jobs_released
               << std::setw(10) << m.jobs_completed
               << std::setw(10) << m.deadline_miss
               << std::setw(12) << m.unfinished
               << std::setw(12) << std::fixed << std::setprecision(3) << m.avg_response_time()
               << std::setw(10) << m.rt_max
               << std::setw(12) << std::fixed << std::setprecision(3) << m.avg_lateness()
               << std::setw(10) << m.lateness_max
               << "\n";
        }

        if (unfinished_total > 0) {
            os << "\n[NOTE] Some jobs did not complete within the simulation horizon.\n"
               << "      Increase the horizon (e.g., hyperperiod) or decide whether unfinished\n"
               << "      jobs should be treated as deadline misses depending on your definition.\n";
        }
    }
};

} // namespace rt