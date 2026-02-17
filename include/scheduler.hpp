// scheduler.hpp
// Created by Francesco on 17/02/2026.
//
// Selezione del job da eseguire secondo politica Fixed Priority Preemptive (FPP).
// Priorità numerica più PICCOLA = priorità più ALTA.

#pragma once

#include <vector>
#include <limits>

#include "job.hpp"
#include "task.hpp"

namespace rt {

    class SchedulerFPP {
    public:
        static int select_job(const std::vector<Job>& jobs,
                              const std::vector<Task>& tasks,
                              tick_t now)
        {
            int selected_index = -1;
            prio_t best_priority = std::numeric_limits<prio_t>::max();

            for (size_t i = 0; i < jobs.size(); ++i) {
                const Job& job = jobs[i];
                if (!job.is_ready(now)) continue;

                const Task& task = tasks.at(job.task_index); // safe
                if (task.priority < best_priority) {
                    best_priority = task.priority;
                    selected_index = static_cast<int>(i);
                }
            }
            return selected_index;
        }
    };

} // namespace rt