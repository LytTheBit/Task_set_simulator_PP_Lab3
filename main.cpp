// main.cpp
// Created by Francesco on 17/02/2026.
//
// Entry point: test del simulatore e (opzionale) batch runner con export CSV.
// Include anche un esempio di generazione automatica di un task set (seeded, utilization-target).

#include <iostream>
#include <vector>
#include <filesystem>

#include "include/simulator.hpp"
#include "include/batch_runner.hpp"
#include "include/taskset_generator.hpp"
#include "include/time_utils.hpp"

int main() {
    using namespace rt;

    // Directory output CSV: sempre nella root del progetto (richiede PROJECT_ROOT_DIR definito via CMake)
    const std::filesystem::path out_dir = std::filesystem::path(PROJECT_ROOT_DIR) / "results";
    std::filesystem::create_directories(out_dir);

    const std::string summary_csv = (out_dir / "summary.csv").string();
    const std::string per_task_csv = (out_dir / "per_task.csv").string();

    // ESEMPIO 1: run singola (debug) - task set hard-coded
    {
        std::vector<Task> tasks = {
            {0, 5, 5, 2, 1, 0},
            {1, 7, 7, 3, 0, 0}
        };

        Simulator sim(tasks, 30);
        sim.run(true, true);
    }

    // ESEMPIO 2: run singola (debug) - task set generato automaticamente
    {
        GeneratorConfig gcfg;
        gcfg.n_tasks = 5;
        gcfg.Tmin = 10;
        gcfg.Tmax = 100;
        gcfg.utilization_target = 0.80;
        gcfg.seed = 42;

        auto tasks = TaskSetGenerator::generate(gcfg);

        // Horizon = iperperiodo per evitare unfinished dovuti a finestra troppo corta
        tick_t horizon = hyperperiod(tasks);

        Simulator sim(tasks, horizon);
        sim.run(true, true);
    }

    // ESEMPIO 3: batch sequenziale con export CSV (task set hard-coded)
    {
        std::vector<std::vector<Task>> tasksets = {
            {
                {0, 5, 5, 2, 1, 0},
                {1, 7, 7, 3, 0, 0}
            },
            {
                {0, 10, 10, 2, 1, 0},
                {1, 15, 15, 5, 0, 0}
            },
            {
                {0, 8, 8, 3, 2, 0},
                {1, 12, 12, 3, 1, 0},
                {2, 20, 20, 4, 0, 0}
            }
        };

        BatchConfig cfg;
        cfg.horizon_mode = HorizonMode::Hyperperiod;  // consigliato per confrontabilità
        cfg.debug_timeline = false;
        cfg.print_input_each_run = false;
        cfg.print_progress = true;

        BatchRunner::run(tasksets, cfg, summary_csv, per_task_csv);
    }

    // ESEMPIO 4: batch sequenziale con export CSV (task set generati automaticamente)
    {
        std::vector<std::vector<Task>> tasksets;
        tasksets.reserve(10);

        // Genera 10 task set riproducibili variando il seed
        for (std::uint32_t s = 1; s <= 10; ++s) {
            GeneratorConfig gcfg;
            gcfg.n_tasks = 8;
            gcfg.Tmin = 10;
            gcfg.Tmax = 200;
            gcfg.utilization_target = 0.85;
            gcfg.seed = 100 + s;

            tasksets.push_back(TaskSetGenerator::generate(gcfg));
        }

        BatchConfig cfg;
        cfg.horizon_mode = HorizonMode::Hyperperiod;
        cfg.debug_timeline = false;
        cfg.print_input_each_run = false;
        cfg.print_progress = true;

        BatchRunner::run(tasksets, cfg, summary_csv, per_task_csv);
    }

    return 0;
}