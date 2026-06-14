// main.cpp
// Created by Francesco on 17/02/2026.
//
// Entry point per confronto batch parallelo e scaling con OpenMP.
// Questa versione confronta diverse scheduling policy OpenMP:
// - static
// - guided
//
// I risultati precedenti erano già stati ottenuti con dynamic,1.
// Quindi questa esecuzione permette di confrontare static/guided
// contro i dati dynamic già disponibili.
//
// Per ogni scheduling policy vengono testati 1, 2, 4, 8 e 12 thread.
// Per ogni configurazione:
// - viene eseguito un warmup
// - vengono eseguite più misure
// - vengono calcolati media, deviazione standard, minimo, massimo,
//   speedup ed efficienza
//
// I file CSV vengono salvati con nomi separati per evitare sovrascritture.

#include <iostream>
#include <vector>
#include <filesystem>
#include <iomanip>
#include <fstream>
#include <numeric>
#include <cmath>
#include <algorithm>
#include <string>

#include "include/batch_runner.hpp"
#include "include/taskset_generator.hpp"

struct ScalingResult {
    std::string schedule_name = "unknown";
    int threads = 1;
    double mean_time = 0.0;
    double stddev_time = 0.0;
    double min_time = 0.0;
    double max_time = 0.0;
    double speedup = 1.0;
    double efficiency = 1.0;
};

struct ScheduleConfig {
    rt::OpenMPSchedule schedule = rt::OpenMPSchedule::Dynamic;
    std::string name = "dynamic";
    int chunk_size = 1;
};

double mean(const std::vector<double>& values) {
    double sum = std::accumulate(values.begin(), values.end(), 0.0);
    return sum / static_cast<double>(values.size());
}

double stddev(const std::vector<double>& values, double avg) {
    if (values.size() <= 1) return 0.0;

    double acc = 0.0;
    for (double v : values) {
        const double diff = v - avg;
        acc += diff * diff;
    }

    return std::sqrt(acc / static_cast<double>(values.size() - 1));
}

void write_scaling_csv(const std::string& path,
                       const std::vector<ScalingResult>& results)
{
    std::ofstream out(path);

    out << "schedule,threads,mean_time_seconds,stddev_time_seconds,"
        << "min_time_seconds,max_time_seconds,speedup,efficiency\n";

    for (const auto& r : results) {
        out << r.schedule_name << ","
            << r.threads << ","
            << std::fixed << std::setprecision(6) << r.mean_time << ","
            << std::fixed << std::setprecision(6) << r.stddev_time << ","
            << std::fixed << std::setprecision(6) << r.min_time << ","
            << std::fixed << std::setprecision(6) << r.max_time << ","
            << std::fixed << std::setprecision(6) << r.speedup << ","
            << std::fixed << std::setprecision(6) << r.efficiency << "\n";
    }
}

void print_scaling_table(const std::string& title,
                         const std::vector<ScalingResult>& results)
{
    std::cout << "\n===== " << title << " =====\n";

    std::cout << std::left
              << std::setw(12) << "Schedule"
              << std::setw(10) << "Threads"
              << std::setw(18) << "Mean (s)"
              << std::setw(18) << "StdDev (s)"
              << std::setw(18) << "Min (s)"
              << std::setw(18) << "Max (s)"
              << std::setw(14) << "Speedup"
              << std::setw(14) << "Efficiency"
              << "\n";

    std::cout << std::string(122, '-') << "\n";

    for (const auto& r : results) {
        std::cout << std::left
                  << std::setw(12) << r.schedule_name
                  << std::setw(10) << r.threads
                  << std::setw(18) << std::fixed << std::setprecision(3) << r.mean_time
                  << std::setw(18) << std::fixed << std::setprecision(3) << r.stddev_time
                  << std::setw(18) << std::fixed << std::setprecision(3) << r.min_time
                  << std::setw(18) << std::fixed << std::setprecision(3) << r.max_time
                  << std::setw(14) << std::fixed << std::setprecision(3) << r.speedup
                  << std::setw(14) << std::fixed << std::setprecision(3) << r.efficiency
                  << "\n";
    }
}

int main() {
    using namespace rt;

    const std::filesystem::path out_dir =
        std::filesystem::path(PROJECT_ROOT_DIR) / "results";
    std::filesystem::create_directories(out_dir);

    // =========================
    // Benchmark parameters
    // =========================
    const int warmup_runs = 1;
    const int measured_runs = 3;

    const std::vector<int> thread_counts = {1, 2, 4, 8, 12};

    // I risultati precedenti erano dynamic,1.
    // Qui testiamo solo static e guided, salvando file separati.
    const std::vector<ScheduleConfig> schedules = {
        {rt::OpenMPSchedule::Static, "static", 1},
        {rt::OpenMPSchedule::Guided, "guided", 1}
    };

    // =========================
    // Output files
    // =========================
    const std::string scaling_all_csv =
        (out_dir / "scaling_static_guided.csv").string();

    const std::string scaling_static_csv =
        (out_dir / "scaling_static.csv").string();

    const std::string scaling_guided_csv =
        (out_dir / "scaling_guided.csv").string();

    const std::string summary_static_csv =
        (out_dir / "summary_static_12t.csv").string();

    const std::string per_task_static_csv =
        (out_dir / "per_task_static_12t.csv").string();

    const std::string summary_guided_csv =
        (out_dir / "summary_guided_12t.csv").string();

    const std::string per_task_guided_csv =
        (out_dir / "per_task_guided_12t.csv").string();

    std::filesystem::remove(scaling_all_csv);
    std::filesystem::remove(scaling_static_csv);
    std::filesystem::remove(scaling_guided_csv);

    std::filesystem::remove(summary_static_csv);
    std::filesystem::remove(per_task_static_csv);
    std::filesystem::remove(summary_guided_csv);
    std::filesystem::remove(per_task_guided_csv);

    // =========================
    // Generazione task set
    // =========================
    // I task set sono generati una sola volta e riutilizzati per tutte le
    // configurazioni. In questo modo static e guided vengono confrontati
    // sullo stesso identico workload.
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

    std::vector<ScalingResult> all_results;
    all_results.reserve(schedules.size() * thread_counts.size());

    std::vector<ScalingResult> static_results;
    std::vector<ScalingResult> guided_results;

    static_results.reserve(thread_counts.size());
    guided_results.reserve(thread_counts.size());

    // =========================
    // Scaling OpenMP per schedule
    // =========================
    for (const auto& schedule_cfg : schedules) {
        std::vector<ScalingResult> current_schedule_results;
        current_schedule_results.reserve(thread_counts.size());

        double baseline_time = 0.0;

        for (int threads : thread_counts) {
            BatchConfig par_cfg = cfg;
            par_cfg.num_threads = threads;
            par_cfg.omp_schedule = schedule_cfg.schedule;
            par_cfg.omp_chunk_size = schedule_cfg.chunk_size;

            std::cout << "\n========================================\n";
            std::cout << "Running OpenMP batch\n";
            std::cout << "Schedule: " << schedule_cfg.name << "\n";
            std::cout << "Threads: " << threads << "\n";
            std::cout << "Chunk size: " << schedule_cfg.chunk_size << "\n";
            std::cout << "Warmup runs: " << warmup_runs << "\n";
            std::cout << "Measured runs: " << measured_runs << "\n";
            std::cout << "========================================\n";

            // Warmup: eseguito ma non usato nelle statistiche.
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

                // Conserviamo l'ultima run per esportare i CSV della
                // configurazione finale a 12 thread.
                last_result = std::move(result);
            }

            const double avg = mean(times);
            const double sd = stddev(times, avg);
            const double min_t = *std::min_element(times.begin(), times.end());
            const double max_t = *std::max_element(times.begin(), times.end());

            // La baseline per ogni schedule è la sua esecuzione a 1 thread.
            // Questo permette di calcolare lo scaling interno di static e guided
            // separatamente.
            if (threads == 1) {
                baseline_time = avg;
            }

            ScalingResult sr;
            sr.schedule_name = schedule_cfg.name;
            sr.threads = threads;
            sr.mean_time = avg;
            sr.stddev_time = sd;
            sr.min_time = min_t;
            sr.max_time = max_t;
            sr.speedup = baseline_time / avg;
            sr.efficiency = sr.speedup / static_cast<double>(threads);

            current_schedule_results.push_back(sr);
            all_results.push_back(sr);

            // Esporta i risultati completi solo per 12 thread.
            // In questo modo puoi verificare che static/guided producano
            // gli stessi risultati logici della versione dynamic.
            if (threads == 12) {
                if (schedule_cfg.name == "static") {
                    BatchRunner::export_batch_result(
                        last_result,
                        summary_static_csv,
                        per_task_static_csv
                    );
                } else if (schedule_cfg.name == "guided") {
                    BatchRunner::export_batch_result(
                        last_result,
                        summary_guided_csv,
                        per_task_guided_csv
                    );
                }
            }
        }

        if (schedule_cfg.name == "static") {
            static_results = current_schedule_results;
        } else if (schedule_cfg.name == "guided") {
            guided_results = current_schedule_results;
        }
    }

    // =========================
    // Stampa tabelle finali
    // =========================
    print_scaling_table("Static schedule results", static_results);
    print_scaling_table("Guided schedule results", guided_results);
    print_scaling_table("All static/guided results", all_results);

    // =========================
    // Salvataggio CSV
    // =========================
    write_scaling_csv(scaling_static_csv, static_results);
    write_scaling_csv(scaling_guided_csv, guided_results);
    write_scaling_csv(scaling_all_csv, all_results);

    std::cout << "\nGenerated files:\n";
    std::cout << "  - " << scaling_static_csv << "\n";
    std::cout << "  - " << scaling_guided_csv << "\n";
    std::cout << "  - " << scaling_all_csv << "\n";
    std::cout << "  - " << summary_static_csv << "\n";
    std::cout << "  - " << per_task_static_csv << "\n";
    std::cout << "  - " << summary_guided_csv << "\n";
    std::cout << "  - " << per_task_guided_csv << "\n";

    std::cout << "\nNote:\n";
    std::cout << "  Previous scaling.csv results correspond to dynamic,1.\n";
    std::cout << "  This run measures static and guided with 1, 2, 4, 8, 12 threads.\n";
    std::cout << "  Compare scaling_static_guided.csv with the previous scaling.csv.\n";

    return 0;
}