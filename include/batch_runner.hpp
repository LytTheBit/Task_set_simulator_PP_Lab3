// batch_runner.hpp
// Created by Francesco on 17/02/2026.
//
// Esecuzione batch sequenziale e parallela di più task set.
// Supporta horizon fisso o iperperiodo, limite massimo all'horizon,
// export CSV, misura dei tempi complessivi del batch e stampa del progresso.
//
// Ottimizzazioni applicate:
// 1. OpenMP schedule configurabile a runtime: static, dynamic, guided.
// 2. Evita copie inutili dei task set nei risultati batch.
//    Ogni BatchRunData mantiene un puntatore costante al task set originale.
// 3. CSV export eseguito dopo il batch, fuori dalla regione parallela,
//    per evitare race condition su file.

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

// Schedule OpenMP usata per distribuire i task set tra i thread.
// Attenzione: questa NON è la politica real-time del simulatore.
// La politica real-time resta FPP/RM. Questa scelta riguarda solo
// la distribuzione del batch tra i thread OpenMP.
enum class OpenMPSchedule {
    Static,
    Dynamic,
    Guided
};

struct BatchConfig {
    HorizonMode horizon_mode = HorizonMode::Fixed;
    tick_t fixed_horizon = 1000;

    // 0 = nessun limite; se > 0 limita l'horizon massimo.
    tick_t max_horizon = 0;

    bool debug_timeline = false;
    bool print_input_each_run = false;
    bool print_summary_each_run = false;
    bool print_progress = true;

    int num_threads = omp_get_max_threads();

    // Schedule OpenMP configurabile.
    // Dynamic con chunk 1 è una buona scelta di default perché i task set
    // possono avere costi diversi.
    OpenMPSchedule omp_schedule = OpenMPSchedule::Dynamic;
    int omp_chunk_size = 1;
};

struct BatchRunData {
    std::int64_t run_id = 0;

    // Puntatore al task set originale.
    // Evita di copiare std::vector<Task> per ogni run.
    //
    // Vincolo importante:
    // il vettore tasksets passato a run_sequential/run_parallel deve rimanere
    // vivo fino alla fine dell'export CSV.
    const std::vector<Task>* tasks = nullptr;

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

    static void configure_omp_schedule(const BatchConfig& cfg) {
        const int chunk_size = std::max(1, cfg.omp_chunk_size);

        switch (cfg.omp_schedule) {
            case OpenMPSchedule::Static:
                omp_set_schedule(omp_sched_static, chunk_size);
                break;

            case OpenMPSchedule::Dynamic:
                omp_set_schedule(omp_sched_dynamic, chunk_size);
                break;

            case OpenMPSchedule::Guided:
                omp_set_schedule(omp_sched_guided, chunk_size);
                break;
        }
    }

    static const char* schedule_name(OpenMPSchedule schedule) {
        switch (schedule) {
            case OpenMPSchedule::Static:
                return "static";

            case OpenMPSchedule::Dynamic:
                return "dynamic";

            case OpenMPSchedule::Guided:
                return "guided";
        }

        return "unknown";
    }

    static void export_results(const BatchExecutionResult& result,
                               const std::string& summary_csv_path,
                               const std::string& per_task_csv_path,
                               const std::string& policy = "FPP")
    {
        for (const auto& run : result.runs) {
            if (run.tasks == nullptr) {
                throw std::runtime_error("BatchRunData contains a null task-set pointer");
            }

            append_summary_csv(summary_csv_path, run.run_id, *run.tasks, run.metrics, policy);
            append_per_task_csv(per_task_csv_path, run.run_id, *run.tasks, run.metrics, policy);
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

            // Non copiamo il task set: salviamo solo un riferimento stabile
            // al task set originale dentro tasksets.
            result.runs[run_id].tasks = &tasks;

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

        configure_omp_schedule(cfg);

        const auto start_time = std::chrono::steady_clock::now();
        std::atomic<int> completed{0};

        // schedule(runtime) usa la policy impostata da configure_omp_schedule().
        // In questo modo static/dynamic/guided possono essere testate dal main
        // senza cambiare questa pragma e senza ricompilare versioni diverse.
        #pragma omp parallel for schedule(runtime) num_threads(cfg.num_threads)
        for (int run_id = 0; run_id < static_cast<int>(tasksets.size()); ++run_id) {
            const auto& tasks = tasksets[run_id];
            const tick_t horizon = resolve_horizon(tasks, cfg);

            // Ogni thread crea un Simulator locale.
            // Lo stato della simulazione non è condiviso tra thread.
            Simulator sim(tasks, horizon);
            sim.run(false, false, false);

            result.runs[run_id].run_id = run_id;

            // Nessuna copia del task set.
            // Ogni run punta al proprio task set originale.
            result.runs[run_id].tasks = &tasks;

            // Ogni thread scrive in una posizione diversa del vettore result.runs.
            // Quindi non c'è race condition sui risultati.
            result.runs[run_id].metrics = sim.metrics();

            const int done = ++completed;

            if (cfg.print_progress) {
                #pragma omp critical
                {
                    std::cout << "\r[PAR|" << cfg.num_threads << "-Thread|"
                              << schedule_name(cfg.omp_schedule) << "] Completed "
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