// main.cpp
// Created by Francesco on 17/02/2026.
//
// Entry point per confronto batch sequenziale/parallelo e scaling con OpenMP.
// Esegue warmup e misure ripetute per ogni configurazione di thread,
// poi esporta CSV e calcola tempo medio, deviazione standard, speedup ed efficienza.

#include <iostream>
#include <vector>
#include <filesystem>
#include <iomanip>
#include <fstream>
#include <numeric>
#include <cmath>
#include <algorithm>

#include "include/batch_runner.hpp"
#include "include/taskset_generator.hpp"

struct ScalingResult {
    int threads = 1;
    double mean_time = 0.0;
    double stddev_time = 0.0;
    double min_time = 0.0;
    double max_time = 0.0;
    double speedup = 1.0;
    double efficiency = 1.0;
};

double mean(const std::vector<double>& values) {
    double sum = std::accumulate(values.begin(), values.end(), 0.0);
    return sum / static_cast<double>(values.size());
}

double stddev(const std::vector<double>& values, double avg) {
    if (values.size() <= 1) return 0.0;

    double acc = 0.0;
    for (double v : values) {
        double diff = v - avg;
        acc += diff * diff;
    }

    return std::sqrt(acc / static_cast<double>(values.size() - 1));
}

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
    // Benchmark parameters
    // =========================
    const int warmup_runs = 1;
    const int measured_runs = 3;

    // Nota: per un test veloce puoi usare {1, 4, 12}
    std::vector<int> thread_counts = {1, 2, 4, 8, 12};

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

    std::vector<ScalingResult> scaling_results;
    scaling_results.reserve(thread_counts.size());

    double baseline_time = 0.0;

    // =========================
    // Scaling OpenMP
    // =========================
    for (int threads : thread_counts) {
        BatchConfig par_cfg = cfg;
        par_cfg.num_threads = threads;

        std::cout << "\n========================================\n";
        std::cout << "Running OpenMP batch with " << threads << " thread(s)\n";
        std::cout << "Warmup runs: " << warmup_runs << "\n";
        std::cout << "Measured runs: " << measured_runs << "\n";
        std::cout << "========================================\n";

        // Warmup: eseguiti ma non usati nelle statistiche
        for (int w = 0; w < warmup_runs; ++w) {
            std::cout << "Warmup " << (w + 1) << "/" << warmup_runs << "\n";
            auto warmup_result = BatchRunner::run_parallel(tasksets, par_cfg);
            (void) warmup_result;
        }

        std::vector<double> times;
        times.reserve(measured_runs);

        BatchExecutionResult last_result;

        for (int r = 0; r < measured_runs; ++r) {
            std::cout << "Measured run " << (r + 1) << "/" << measured_runs << "\n";
            auto result = BatchRunner::run_parallel(tasksets, par_cfg);

            times.push_back(result.elapsed_seconds);
            last_result = std::move(result);
        }

        const double avg = mean(times);
        const double sd = stddev(times, avg);
        const double min_t = *std::min_element(times.begin(), times.end());
        const double max_t = *std::max_element(times.begin(), times.end());

        if (threads == 1) {
            baseline_time = avg;

            // Esporta anche la baseline a 1 thread come riferimento sequenziale/OpenMP-1T
            BatchRunner::export_batch_result(last_result, summary_seq_csv, per_task_seq_csv);
        }

        ScalingResult sr;
        sr.threads = threads;
        sr.mean_time = avg;
        sr.stddev_time = sd;
        sr.min_time = min_t;
        sr.max_time = max_t;
        sr.speedup = baseline_time / avg;
        sr.efficiency = sr.speedup / static_cast<double>(threads);

        scaling_results.push_back(sr);

        // Esporta i CSV completi solo per la configurazione finale a 12 thread
        if (threads == 12) {
            BatchRunner::export_batch_result(last_result, summary_par_csv, per_task_par_csv);
        }
    }

    // =========================
    // Stampa tabella finale
    // =========================
    std::cout << "\n===== Scaling results =====\n";
    std::cout << std::left
              << std::setw(10) << "Threads"
              << std::setw(18) << "Mean (s)"
              << std::setw(18) << "StdDev (s)"
              << std::setw(18) << "Min (s)"
              << std::setw(18) << "Max (s)"
              << std::setw(14) << "Speedup"
              << std::setw(14) << "Efficiency"
              << "\n";

    std::cout << std::string(110, '-') << "\n";

    for (const auto& r : scaling_results) {
        std::cout << std::left
                  << std::setw(10) << r.threads
                  << std::setw(18) << std::fixed << std::setprecision(3) << r.mean_time
                  << std::setw(18) << std::fixed << std::setprecision(3) << r.stddev_time
                  << std::setw(18) << std::fixed << std::setprecision(3) << r.min_time
                  << std::setw(18) << std::fixed << std::setprecision(3) << r.max_time
                  << std::setw(14) << std::fixed << std::setprecision(3) << r.speedup
                  << std::setw(14) << std::fixed << std::setprecision(3) << r.efficiency
                  << "\n";
    }

    // =========================
    // Salvataggio scaling.csv
    // =========================
    {
        std::ofstream out(scaling_csv);
        out << "threads,mean_time_seconds,stddev_time_seconds,min_time_seconds,max_time_seconds,speedup,efficiency\n";

        for (const auto& r : scaling_results) {
            out << r.threads << ","
                << std::fixed << std::setprecision(6) << r.mean_time << ","
                << std::fixed << std::setprecision(6) << r.stddev_time << ","
                << std::fixed << std::setprecision(6) << r.min_time << ","
                << std::fixed << std::setprecision(6) << r.max_time << ","
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