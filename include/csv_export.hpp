// csv_export.hpp
// Created by Francesco on 17/02/2026.
//
// Funzioni di esportazione CSV:
// - summary per simulazione (una riga per task set)
// - per-task metrics (una riga per task per simulazione)

#pragma once

#include <fstream>
#include <string>
#include <vector>
#include <stdexcept>
#include <iomanip>

#include "task.hpp"
#include "metrics.hpp"

namespace rt {

inline void write_csv_header_if_needed(std::ofstream& out, const std::string& header) {
    // Assumiamo che se il file è vuoto (tellp == 0) dobbiamo scrivere l'header.
    if (out.tellp() == 0) {
        out << header << "\n";
    }
}

inline void append_summary_csv(const std::string& path,
                               std::int64_t run_id,
                               const std::vector<Task>& tasks,
                               const SimulationMetrics& m,
                               const std::string& policy = "FPP")
{
    std::ofstream out(path, std::ios::app);
    if (!out) throw std::runtime_error("Cannot open CSV file: " + path);

    write_csv_header_if_needed(out,
        "run_id,policy,n_tasks,horizon,busy_ticks,utilization,deadline_miss,unfinished_jobs");

    out << run_id << ","
        << policy << ","
        << tasks.size() << ","
        << m.horizon << ","
        << m.busy_ticks << ","
        << std::fixed << std::setprecision(6) << m.utilization() << ","
        << m.deadline_miss_total << ","
        << m.unfinished_total
        << "\n";
}

inline void append_per_task_csv(const std::string& path,
                               std::int64_t run_id,
                               const std::vector<Task>& tasks,
                               const SimulationMetrics& m,
                               const std::string& policy = "FPP")
{
    std::ofstream out(path, std::ios::app);
    if (!out) throw std::runtime_error("Cannot open CSV file: " + path);

    write_csv_header_if_needed(out,
        "run_id,policy,task_index,task_id,priority,period,deadline,wcet,offset,"
        "jobs_released,jobs_completed,deadline_miss,unfinished,rt_avg,rt_max,late_avg,late_max");

    for (size_t i = 0; i < tasks.size(); ++i) {
        const auto& t = tasks[i];
        const auto& tm = m.per_task[i];

        out << run_id << ","
            << policy << ","
            << i << ","
            << t.id << ","
            << t.priority << ","
            << t.period << ","
            << t.deadline << ","
            << t.wcet << ","
            << t.offset << ","
            << tm.jobs_released << ","
            << tm.jobs_completed << ","
            << tm.deadline_miss << ","
            << tm.unfinished << ","
            << std::fixed << std::setprecision(6) << tm.avg_response_time() << ","
            << tm.rt_max << ","
            << std::fixed << std::setprecision(6) << tm.avg_lateness() << ","
            << tm.lateness_max
            << "\n";
    }
}

} // namespace rt