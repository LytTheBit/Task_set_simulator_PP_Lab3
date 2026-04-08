// main.cpp
// Created by Francesco on 17/02/2026.
//
// Entry point per confronto batch sequenziale/parallelo e scaling con OpenMP.
// Genera task set, esegue il batch con diversi numeri di thread, esporta i CSV
// e misura tempo, speedup ed efficienza.

#include <iostream>
#include <vector>
#include <filesystem>
#include <iomanip>
#include <fstream>

#include "include/batch_runner.hpp"
#include "include/taskset_generator.hpp"

struct ScalingResult {
    int threads = 1;
    double time_seconds = 0.0;
    double speedup = 1.0;
    double efficiency = 1.0;
};

int main() {
    using namespace rt;

    const std::filesystem::path out_dir =
        std::filesystem::path(PROJECT_ROOT_DIR) / "results";
    std::filesystem::create_directories(out_dir);

    const std::string summary_seq_csv = (out_dir / "summary_seq.csv").string();
    const std::string per_task_seq_csv = (out_dir / "per_task_seq.csv").string();

    const std::string summary_par_csv = (out_dir / "summary_par.csv").string();
    const std::string per_task_par_csv = (out_dir / "per_task_par.csv").string();

    const std::string scaling_csv = (out_dir / "scaling.csv").string();

    std::filesystem::remove(summary_seq_csv);
    std::filesystem::remove(per_task_seq_csv);
    std::filesystem::remove(summary_par_csv);
    std::filesystem::remove(per_task_par_csv);
    std::filesystem::remove(scaling_csv);

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
    // Configurazione comune
    // =========================
    BatchConfig cfg;
    cfg.horizon_mode = HorizonMode::Hyperperiod;
    cfg.max_horizon = 200000;
    cfg.debug_timeline = false;
    cfg.print_input_each_run = false;
    cfg.print_summary_each_run = false;
    cfg.print_progress = true;

    // =========================
    // Baseline sequenziale
    // =========================
    std::cout << "Starting sequential baseline...\n";
    auto seq_result = BatchRunner::run_sequential(tasksets, cfg);
    BatchRunner::export_batch_result(seq_result, summary_seq_csv, per_task_seq_csv);

    const double t_seq = seq_result.elapsed_seconds;

    // =========================
    // Scaling OpenMP
    // =========================
    std::vector<int> thread_counts = {1, 2, 4, 8, 12};
    std::vector<ScalingResult> scaling_results;
    scaling_results.reserve(thread_counts.size());

    for (int threads : thread_counts) {
        BatchConfig par_cfg = cfg;
        par_cfg.num_threads = threads;

        std::cout << "Running parallel batch with " << threads << " thread(s)...\n";
        auto par_result = BatchRunner::run_parallel(tasksets, par_cfg);

        // Esporta i CSV completi solo per la configurazione finale a 12 thread
        if (threads == 12) {
            BatchRunner::export_batch_result(par_result, summary_par_csv, per_task_par_csv);
        }

        ScalingResult r;
        r.threads = threads;
        r.time_seconds = par_result.elapsed_seconds;
        r.speedup = t_seq / r.time_seconds;
        r.efficiency = r.speedup / static_cast<double>(threads);

        scaling_results.push_back(r);
    }

    // =========================
    // Stampa tabella finale
    // =========================
    std::cout << "\n===== Scaling results =====\n";
    std::cout << std::left
              << std::setw(10) << "Threads"
              << std::setw(18) << "Time (s)"
              << std::setw(14) << "Speedup"
              << std::setw(14) << "Efficiency"
              << "\n";

    std::cout << std::string(56, '-') << "\n";

    for (const auto& r : scaling_results) {
        std::cout << std::left
                  << std::setw(10) << r.threads
                  << std::setw(18) << std::fixed << std::setprecision(3) << r.time_seconds
                  << std::setw(14) << std::fixed << std::setprecision(3) << r.speedup
                  << std::setw(14) << std::fixed << std::setprecision(3) << r.efficiency
                  << "\n";
    }

    // =========================
    // Salvataggio scaling.csv
    // =========================
    {
        std::ofstream out(scaling_csv);
        out << "threads,time_seconds,speedup,efficiency\n";
        for (const auto& r : scaling_results) {
            out << r.threads << ","
                << std::fixed << std::setprecision(6) << r.time_seconds << ","
                << std::fixed << std::setprecision(6) << r.speedup << ","
                << std::fixed << std::setprecision(6) << r.efficiency << "\n";
        }
    }

    std::cout << "\nGenerated files:\n";
    std::cout << "  - " << summary_seq_csv << "\n";
    std::cout << "  - " << per_task_seq_csv << "\n";
    std::cout << "  - " << summary_par_csv << "\n";
    std::cout << "  - " << per_task_par_csv << "\n";
    std::cout << "  - " << scaling_csv << "\n";

    return 0;
}