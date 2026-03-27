// batch_runner.hpp
// Created by Francesco on 17/02/2026.
//
// Esecuzione batch (sequenziale) di più task set.
// Calcola automaticamente l'horizon (fisso o iperperiodo) e salva i risultati su CSV.

#pragma once

#include <vector>
#include <string>
#include <cstdint>
#include <iostream>

#include "task.hpp"
#include "simulator.hpp"
#include "time_utils.hpp"
#include "csv_export.hpp"
#include <chrono>
#include <iomanip>
#include <sstream>
#include <algorithm>

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

        // aggiorna la riga di progresso ogni N run
        std::size_t progress_every_runs = 1;
    };

    class BatchRunner {
    public:
        static void run(const std::vector<std::vector<Task>>& tasksets,
                        const BatchConfig& cfg,
                        const std::string& summary_csv_path,
                        const std::string& per_task_csv_path)
        {
            std::int64_t run_id = 0;

            for (const auto& tasks : tasksets) {
                tick_t horizon = cfg.fixed_horizon;

                if (cfg.horizon_mode == HorizonMode::Hyperperiod) {
                    horizon = hyperperiod(tasks);
                }

                Simulator sim(tasks, horizon);
                sim.run(cfg.debug_timeline, cfg.print_input_each_run);

                // Salvataggio CSV
                append_summary_csv(summary_csv_path, run_id, tasks, sim.metrics(), "FPP");
                append_per_task_csv(per_task_csv_path, run_id, tasks, sim.metrics(), "FPP");

                if (cfg.print_progress) {
                    std::cout << "[Batch] Completed run_id=" << run_id
                              << " (n_tasks=" << tasks.size()
                              << ", horizon=" << horizon << ")\n";
                }
                run_id++;
            }
        }
    };

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
        if (seconds < 0.0) seconds = 0.0;

        auto total = static_cast<long long>(seconds + 0.5);
        const long long h = total / 3600;
        total %= 3600;
        const long long m = total / 60;
        const long long s = total % 60;

        std::ostringstream oss;
        if (h > 0) oss << h << "h ";
        if (h > 0 || m > 0) oss << m << "m ";
        oss << s << "s";
        return oss.str();
    }

    static void print_progress_line(std::int64_t runs_done,
                                    std::int64_t runs_total,
                                    tick_t ticks_done,
                                    tick_t total_ticks,
                                    const std::chrono::steady_clock::time_point& start)
    {
        const auto now = std::chrono::steady_clock::now();
        const double elapsed =
            std::chrono::duration_cast<std::chrono::duration<double>>(now - start).count();

        const double progress =
            (total_ticks > 0) ? (100.0 * static_cast<double>(ticks_done) / static_cast<double>(total_ticks))
                              : 100.0;

        const double tick_rate =
            (elapsed > 0.0) ? (static_cast<double>(ticks_done) / elapsed) : 0.0;

        const double eta =
            (tick_rate > 0.0) ? (static_cast<double>(total_ticks - ticks_done) / tick_rate) : 0.0;

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
                    const std::string& per_task_csv_path)
    {
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
        const auto start = std::chrono::steady_clock::now();

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
                const bool must_print =
                    ((run_id + 1) % static_cast<std::int64_t>(std::max<std::size_t>(1, cfg.progress_every_runs)) == 0) ||
                    (run_id + 1 == static_cast<std::int64_t>(tasksets.size()));

                if (must_print) {
                    print_progress_line(run_id + 1,
                                        static_cast<std::int64_t>(tasksets.size()),
                                        ticks_done,
                                        total_ticks,
                                        start);
                }
            }
        }

        if (cfg.print_progress) {
            std::cout << "\n[Batch] Completed.\n";
        }
    }

} // namespace rt

