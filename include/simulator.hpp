// simulator.hpp
// Created by Francesco on 17/02/2026.
//
// Simulatore tick-based per task real-time con politica FPP.
// Produce timeline di debug e calcola deadline miss in modo coerente con finish_time.

#pragma once

#include <vector>
#include <iostream>

#include "task.hpp"
#include "job.hpp"
#include "scheduler.hpp"

namespace rt {

class Simulator {
public:
    Simulator(std::vector<Task> tasks, tick_t horizon)
        : tasks_(std::move(tasks)), horizon_(horizon) {}

    void run(bool debug_timeline = false) {
        reset();

        for (tick_t t = 0; t < horizon_; ++t) {

            // 1) Release nuovi job
            for (std::int32_t ti = 0; ti < static_cast<std::int32_t>(tasks_.size()); ++ti) {
                const auto& task = tasks_[ti];
                if (task.releases_at(t)) {
                    Job j = Job::from_task(task, ti, t, job_counter_[ti]++);
                    jobs_.push_back(j);
                }
            }

            // 2) Selezione job (FPP) e 3) esecuzione
            int idx = SchedulerFPP::select_job(jobs_, tasks_, t);

            if (idx >= 0) {
                jobs_[idx].execute_one_tick(t);
                busy_ticks_++;

                // Se il job ha appena finito, valuta deadline miss
                if (jobs_[idx].finish_time.has_value()) {
                    if (*jobs_[idx].finish_time > jobs_[idx].abs_deadline) {
                        deadline_miss_++;
                    }
                }

                if (debug_timeline) {
                    std::cout << "t=" << t
                              << " running task " << jobs_[idx].task_id
                              << " (job " << jobs_[idx].job_index << ")\n";
                }
            } else {
                if (debug_timeline) {
                    std::cout << "t=" << t << " idle\n";
                }
            }
        }
    }

    int deadline_miss() const { return deadline_miss_; }

    double utilization() const {
        return horizon_ > 0 ? static_cast<double>(busy_ticks_) / horizon_ : 0.0;
    }

private:
    void reset() {
        jobs_.clear();
        deadline_miss_ = 0;
        busy_ticks_ = 0;
        job_counter_.assign(tasks_.size(), 0);
    }

private:
    std::vector<Task> tasks_;
    std::vector<Job> jobs_;
    tick_t horizon_;

    int deadline_miss_ = 0;
    tick_t busy_ticks_ = 0;

    std::vector<int> job_counter_; // per task_index
};

} // namespace rt
