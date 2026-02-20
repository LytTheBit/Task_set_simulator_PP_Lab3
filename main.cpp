// main.cpp
// Created by Francesco on 17/02/2026.
//
// Entry point: test del simulatore e (opzionale) batch runner con export CSV.

#include <iostream>
#include <vector>

#include "include/simulator.hpp"
#include "include/batch_runner.hpp"

int main() {
    using namespace rt;

    // ESEMPIO 1: run singola (debug)
    {
        std::vector<Task> tasks = {
            {0, 5, 5, 2, 1, 0},
            {1, 7, 7, 3, 0, 0}
        };

        Simulator sim(tasks, 30);
        sim.run(true, true);
    }

    // ESEMPIO 2: batch sequenziale con export CSV
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

        // Assicurati che la cartella "results" esista (per ora creala a mano).
        BatchRunner::run(tasksets, cfg,
                         "results/summary.csv",
                         "results/per_task.csv");
    }

    return 0;
}