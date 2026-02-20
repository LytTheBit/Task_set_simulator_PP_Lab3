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

namespace rt {

    enum class HorizonMode {
        Fixed,
        Hyperperiod
    };

    struct BatchConfig {
        HorizonMode horizon_mode = HorizonMode::Fixed;
        tick_t fixed_horizon = 1000;          // usato se Fixed
        bool debug_timeline = false;          // normalmente false in batch
        bool print_input_each_run = false;    // normalmente false in batch
        bool print_progress = true;
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

} // namespace rt