// simulator.hpp
// Created by Francesco on 17/02/2026.
//
// Simulatore tick-based per task real-time
// con politica Fixed Priority Preemptive.
// Include timeline di debug e conteggio deadline miss.

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
            for (auto& task : tasks_) {
                if (task.releases_at(t)) {
                    Job j = Job::from_task(task, t, job_counter_[task.id]++);
                    jobs_.push_back(j);
                }
            }

            // 2) Deadline check
            for (auto& job : jobs_) {
                if (!job.is_completed() && job.abs_deadline == t) {
                    deadline_miss_++;
                }
            }

            // 3) Selezione job (FPP)
            int idx = SchedulerFPP::select_job(jobs_, tasks_, t);

            if (idx >= 0) {
                jobs_[idx].execute_one_tick(t);
                busy_ticks_++;

                if (debug_timeline) {
                    std::cout << "t=" << t
                              << " running task "
                              << jobs_[idx].task_id
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
        return static_cast<double>(busy_ticks_) / horizon_;
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

    std::vector<int> job_counter_; // numero job creati per task
};

} // namespace rt
