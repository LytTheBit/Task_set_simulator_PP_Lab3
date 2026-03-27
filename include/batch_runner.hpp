// batch_runner.hpp
// Created by Francesco on 17/02/2026.
//
// Esecuzione batch (sequenziale) di più task set.
// Supporta horizon fisso o iperperiodo, limite massimo all'horizon,
// export CSV e progresso sintetico con stima ETA.

#pragma once

#include <vector>
#include <string>
#include <cstdint>
#include <iostream>
#include <chrono>
#include <iomanip>
#include <sstream>
#include <algorithm>
#include <stdexcept>

#include "task.hpp"
#include "simulator.hpp"
#include "time_utils.hpp"
#include "csv_export.hpp"

namespace rt {

enum class HorizonMode {
    Fixed,
    Hyperperiod
};

struct BatchConfig {
    HorizonMode horizon_mode = HorizonMode::Fixed;
    tick_t fixed_horizon = 1000;

    // 0 = nessun limite; se > 0 limita l'horizon massimo
    tick_t max_horizon = 0;

    bool debug_timeline = false;
    bool print_input_each_run = false;
    bool print_summary_each_run = false;
    bool print_progress = true;

    // Stampa il progresso ogni N run completate
    std::size_t progress_every_runs = 1;
};

class BatchRunner {
private:
    static tick_t resolve_horizon(const std::vector<Task>& tasks, const BatchConfig& cfg) {
        tick_t horizon = cfg.fixed_horizon;

        if (cfg.horizon_mode == HorizonMode::Hyperperiod) {
            try {
                horizon = hyperperiod(tasks);
            } catch (const std::overflow_error&) {
                if (cfg.max_horizon > 0) {
                    horizon = cfg.max_horizon;
                } else {
                    throw;
                }
            }
        }

        if (cfg.max_horizon > 0) {
            horizon = std::min(horizon, cfg.max_horizon);
        }

        return horizon;
    }

    static std::string format_seconds(double seconds) {
        if (seconds < 0.0) {
            seconds = 0.0;
        }

        auto total = static_cast<long long>(seconds + 0.5);
        const long long hours = total / 3600;
        total %= 3600;
        const long long minutes = total / 60;
        const long long secs = total % 60;

        std::ostringstream oss;
        if (hours > 0) {
            oss << hours << "h ";
        }
        if (hours > 0 || minutes > 0) {
            oss << minutes << "m ";
        }
        oss << secs << "s";
        return oss.str();
    }

    static void print_progress_line(std::int64_t runs_done,
                                    std::int64_t runs_total,
                                    tick_t ticks_done,
                                    tick_t total_ticks,
                                    const std::chrono::steady_clock::time_point& start_time) {
        const auto now = std::chrono::steady_clock::now();
        const double elapsed =
            std::chrono::duration_cast<std::chrono::duration<double>>(now - start_time).count();

        const double progress =
            (total_ticks > 0)
                ? (100.0 * static_cast<double>(ticks_done) / static_cast<double>(total_ticks))
                : 100.0;

        const double tick_rate =
            (elapsed > 0.0) ? (static_cast<double>(ticks_done) / elapsed) : 0.0;

        const double eta =
            (tick_rate > 0.0)
                ? (static_cast<double>(total_ticks - ticks_done) / tick_rate)
                : 0.0;

        std::cout << "\r[Batch] "
                  << runs_done << "/" << runs_total
                  << " runs"
                  << " | ticks " << ticks_done << "/" << total_ticks
                  << " | " << std::fixed << std::setprecision(1) << progress << "%"
                  << " | ETA " << format_seconds(eta)
                  << std::flush;
    }

public:
    static void run(const std::vector<std::vector<Task>>& tasksets,
                    const BatchConfig& cfg,
                    const std::string& summary_csv_path,
                    const std::string& per_task_csv_path) {
        if (tasksets.empty()) {
            std::cout << "[Batch] No task sets to run.\n";
            return;
        }

        std::vector<tick_t> horizons(tasksets.size(), 0);
        tick_t total_ticks = 0;

        for (size_t i = 0; i < tasksets.size(); ++i) {
            horizons[i] = resolve_horizon(tasksets[i], cfg);
            total_ticks += horizons[i];
        }

        tick_t ticks_done = 0;
        const auto start_time = std::chrono::steady_clock::now();

        for (std::int64_t run_id = 0; run_id < static_cast<std::int64_t>(tasksets.size()); ++run_id) {
            const auto& tasks = tasksets[run_id];
            const tick_t horizon = horizons[run_id];

            Simulator sim(tasks, horizon);
            sim.run(cfg.debug_timeline,
                    cfg.print_input_each_run,
                    cfg.print_summary_each_run);

            append_summary_csv(summary_csv_path, run_id, tasks, sim.metrics(), "FPP");
            append_per_task_csv(per_task_csv_path, run_id, tasks, sim.metrics(), "FPP");

            ticks_done += horizon;

            if (cfg.print_progress) {
                const std::size_t step = std::max<std::size_t>(1, cfg.progress_every_runs);
                const bool must_print =
                    (((run_id + 1) % static_cast<std::int64_t>(step)) == 0) ||
                    (run_id + 1 == static_cast<std::int64_t>(tasksets.size()));

                if (must_print) {
                    print_progress_line(run_id + 1,
                                        static_cast<std::int64_t>(tasksets.size()),
                                        ticks_done,
                                        total_ticks,
                                        start_time);
                }
            }
        }

        if (cfg.print_progress) {
            std::cout << "\n[Batch] Completed.\n";
        }
    }
};

} // namespace rt