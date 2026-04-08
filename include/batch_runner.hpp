// batch_runner.hpp
// Created by Francesco on 17/02/2026.
//
// Esecuzione batch sequenziale e parallela di più task set.
// Supporta horizon fisso o iperperiodo, limite massimo all'horizon,
// export CSV, misura dei tempi complessivi del batch e stampa del progresso.

#pragma once

#include <vector>
#include <string>
#include <cstdint>
#include <iostream>
#include <chrono>
#include <algorithm>
#include <stdexcept>
#include <atomic>
#include <omp.h>

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

    int num_threads = omp_get_max_threads();
};

struct BatchRunData {
    std::int64_t run_id = 0;
    std::vector<Task> tasks;
    SimulationMetrics metrics;
};

struct BatchExecutionResult {
    std::vector<BatchRunData> runs;
    double elapsed_seconds = 0.0;
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

    static void export_results(const BatchExecutionResult& result,
                               const std::string& summary_csv_path,
                               const std::string& per_task_csv_path,
                               const std::string& policy = "FPP")
    {
        for (const auto& run : result.runs) {
            append_summary_csv(summary_csv_path, run.run_id, run.tasks, run.metrics, policy);
            append_per_task_csv(per_task_csv_path, run.run_id, run.tasks, run.metrics, policy);
        }
    }

public:
    static BatchExecutionResult run_sequential(const std::vector<std::vector<Task>>& tasksets,
                                               const BatchConfig& cfg)
    {
        BatchExecutionResult result;
        result.runs.resize(tasksets.size());

        const auto start_time = std::chrono::steady_clock::now();

        for (std::int64_t run_id = 0; run_id < static_cast<std::int64_t>(tasksets.size()); ++run_id) {
            const auto& tasks = tasksets[run_id];
            const tick_t horizon = resolve_horizon(tasks, cfg);

            Simulator sim(tasks, horizon);
            sim.run(cfg.debug_timeline,
                    cfg.print_input_each_run,
                    cfg.print_summary_each_run);

            result.runs[run_id].run_id = run_id;
            result.runs[run_id].tasks = tasks;
            result.runs[run_id].metrics = sim.metrics();

            if (cfg.print_progress) {
                std::cout << "\r[SEQ] Completed " << (run_id + 1)
                          << "/" << tasksets.size() << std::flush;
            }
        }

        const auto end_time = std::chrono::steady_clock::now();
        result.elapsed_seconds =
            std::chrono::duration_cast<std::chrono::duration<double>>(end_time - start_time).count();

        if (cfg.print_progress) {
            std::cout << "\n";
        }

        return result;
    }

    static BatchExecutionResult run_parallel(const std::vector<std::vector<Task>>& tasksets,
                                             const BatchConfig& cfg)
    {
        BatchExecutionResult result;
        result.runs.resize(tasksets.size());

        const auto start_time = std::chrono::steady_clock::now();
        std::atomic<int> completed{0};

        #pragma omp parallel for schedule(dynamic, 1) num_threads(cfg.num_threads)
        for (int run_id = 0; run_id < static_cast<int>(tasksets.size()); ++run_id) {
            const auto& tasks = tasksets[run_id];
            const tick_t horizon = resolve_horizon(tasks, cfg);

            Simulator sim(tasks, horizon);
            sim.run(false, false, false);

            result.runs[run_id].run_id = run_id;
            result.runs[run_id].tasks = tasks;
            result.runs[run_id].metrics = sim.metrics();

            int done = ++completed;

            if (cfg.print_progress) {
                #pragma omp critical
                {
                    std::cout << "\r[PAR|" << cfg.num_threads << "-Thread] Completed "
                              << done << "/" << tasksets.size() << std::flush;
                }
            }
        }

        const auto end_time = std::chrono::steady_clock::now();
        result.elapsed_seconds =
            std::chrono::duration_cast<std::chrono::duration<double>>(end_time - start_time).count();

        if (cfg.print_progress) {
            std::cout << "\n";
        }

        return result;
    }

    static void run_and_export_sequential(const std::vector<std::vector<Task>>& tasksets,
                                          const BatchConfig& cfg,
                                          const std::string& summary_csv_path,
                                          const std::string& per_task_csv_path)
    {
        auto result = run_sequential(tasksets, cfg);
        export_results(result, summary_csv_path, per_task_csv_path, "FPP");
    }

    static void run_and_export_parallel(const std::vector<std::vector<Task>>& tasksets,
                                        const BatchConfig& cfg,
                                        const std::string& summary_csv_path,
                                        const std::string& per_task_csv_path)
    {
        auto result = run_parallel(tasksets, cfg);
        export_results(result, summary_csv_path, per_task_csv_path, "FPP");
    }

    static void export_batch_result(const BatchExecutionResult& result,
                                    const std::string& summary_csv_path,
                                    const std::string& per_task_csv_path)
    {
        export_results(result, summary_csv_path, per_task_csv_path, "FPP");
    }
};

} // namespace rt