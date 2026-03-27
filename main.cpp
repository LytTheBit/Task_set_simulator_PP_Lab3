// main.cpp
// Created by Francesco on 17/02/2026.
//
// Entry point per esecuzione batch silenziosa con export CSV e progresso sintetico.
// Pensato per campagne lunghe: niente output dettagliato su terminale, solo avanzamento batch.

#include <iostream>
#include <vector>
#include <filesystem>
#include <cstdint>

#include "include/batch_runner.hpp"
#include "include/taskset_generator.hpp"

int main() {
    using namespace rt;

    // Directory di output nella root del progetto.
    // Richiede PROJECT_ROOT_DIR definito via CMake.
    const std::filesystem::path out_dir =
        std::filesystem::path(PROJECT_ROOT_DIR) / "results";

    std::filesystem::create_directories(out_dir);

    const std::string summary_csv = (out_dir / "summary.csv").string();
    const std::string per_task_csv = (out_dir / "per_task.csv").string();

    // Rimuove eventuali file precedenti per evitare di accumulare righe vecchie.
    std::filesystem::remove(summary_csv);
    std::filesystem::remove(per_task_csv);

    // =========================
    // Generazione task set
    // =========================
    std::vector<std::vector<Task>> tasksets;
    tasksets.reserve(200);

    for (std::uint32_t s = 1; s <= 200; ++s) {
        GeneratorConfig gcfg;
        gcfg.n_tasks = 8;
        gcfg.Tmin = 10;
        gcfg.Tmax = 150;
        gcfg.utilization_target = 0.85;
        gcfg.seed = 1000 + s;

        tasksets.push_back(TaskSetGenerator::generate(gcfg));
    }

    // =========================
    // Configurazione batch
    // =========================
    BatchConfig cfg;
    cfg.horizon_mode = HorizonMode::Hyperperiod;

    // Limite massimo all'horizon per evitare iperperiodi ingestibili.
    cfg.max_horizon = 200000;

    // Nessun output dettagliato durante le singole run.
    cfg.debug_timeline = false;
    cfg.print_input_each_run = false;
    cfg.print_summary_each_run = false;

    // Solo avanzamento complessivo batch.
    cfg.print_progress = true;
    cfg.progress_every_runs = 1;

    std::cout << "Starting batch execution...\n";
    std::cout << "Output directory: " << out_dir.string() << "\n";
    std::cout << "Task sets: " << tasksets.size() << "\n";
    std::cout << "Policy: FPP\n";
    std::cout << "Horizon mode: Hyperperiod (capped at " << cfg.max_horizon << " ticks)\n\n";

    BatchRunner::run(tasksets, cfg, summary_csv, per_task_csv);

    std::cout << "\nBatch finished.\n";
    std::cout << "Generated files:\n";
    std::cout << "  - " << summary_csv << "\n";
    std::cout << "  - " << per_task_csv << "\n";

    return 0;
}