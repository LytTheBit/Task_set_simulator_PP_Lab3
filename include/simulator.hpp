// simulator.hpp
// Created by Francesco on 17/02/2026.
//
// Simulatore tick-based per task real-time con politica FPP.
// Raccoglie metriche per-task e globali:
// - response time
// - lateness
// - deadline miss
// - utilization
// - unfinished jobs
//
// Ottimizzazione applicata:
// invece di mantenere tutti i job mai rilasciati, il simulatore mantiene
// solo i job ancora attivi in active_jobs_.
//
// Questo riduce il costo della selezione del job a ogni tick, perché lo
// scheduler non deve più scansionare anche i job già completati.
//
// La politica real-time NON cambia:
// resta Fixed Priority Preemptive con priorità RM/FPP.

#pragma once

#include <vector>
#include <iostream>
#include <iomanip>

#include "task.hpp"
#include "job.hpp"
#include "scheduler.hpp"
#include "metrics.hpp"

namespace rt {

class Simulator {
public:
    Simulator(std::vector<Task> tasks, tick_t horizon)
        : tasks_(std::move(tasks)), horizon_(horizon)
    {
        for (auto& t : tasks_) {
            t.validate();
        }

        metrics_.init_from_tasks(tasks_, horizon_);
    }

    void run(bool debug_timeline = false,
             bool print_input = true,
             bool print_summary = true)
    {
        reset();

        if (print_input) {
            print_taskset(std::cout);
            std::cout << "Policy: Fixed Priority Preemptive (FPP)\n";
            std::cout << "Horizon: " << horizon_ << " ticks (1 tick = 1 ms)\n\n";
        }

        for (tick_t t = 0; t < horizon_; ++t) {

            // 1) Release dei nuovi job.
            //
            // Ogni job rilasciato viene inserito in active_jobs_.
            // Questo vettore contiene solo job non ancora completati.
            for (std::int32_t ti = 0; ti < static_cast<std::int32_t>(tasks_.size()); ++ti) {
                const auto& task = tasks_[ti];

                if (task.releases_at(t)) {
                    Job j = Job::from_task(task, ti, t, job_counter_[ti]++);

                    active_jobs_.push_back(j);
                    metrics_.per_task[ti].on_job_released();
                }
            }

            // 2) Selezione del job secondo FPP/RM.
            //
            // Prima veniva passato un vettore con tutti i job mai rilasciati.
            // Ora viene passato solo active_jobs_, quindi lo scheduler scansiona
            // un insieme più piccolo.
            int idx = SchedulerFPP::select_job(active_jobs_, tasks_, t);

            // 3) Esecuzione di un tick.
            if (idx >= 0) {
                Job& running = active_jobs_[idx];

                running.execute_one_tick(t);
                metrics_.busy_ticks++;

                const bool completed_now = running.finish_time.has_value();

                if (completed_now) {
                    metrics_.per_task[running.task_index].on_job_completed(running);
                }

                if (debug_timeline) {
                    print_timeline_line(std::cout, t, running);
                }

                // 4) Rimozione dei job completati.
                //
                // Il job completato non serve più allo scheduler nei tick
                // successivi. Rimuoverlo evita che venga ricontrollato a ogni
                // iterazione futura.
                //
                // Uso erase invece di swap-and-pop per preservare l'ordine dei
                // job attivi. È una scelta più sicura per non alterare eventuali
                // tie-break impliciti nella selezione dello scheduler.
                if (completed_now) {
                    active_jobs_.erase(active_jobs_.begin() + idx);
                }
            } else {
                if (debug_timeline) {
                    std::cout << "t=" << std::setw(4) << t << "  IDLE\n";
                }
            }
        }

        if (debug_timeline) {
            int count = 0;

            // A fine simulazione active_jobs_ contiene esattamente i job
            // rilasciati ma non ancora completati entro l'horizon.
            for (const auto& j : active_jobs_) {
                if (!j.is_completed()) {
                    if (count == 0) {
                        std::cout << "\nUnfinished jobs at end of horizon:\n";
                    }

                    std::cout << "  " << j.to_string() << "\n";
                    count++;
                }
            }
        }

        metrics_.finalize();

        if (print_summary) {
            metrics_.print_summary(std::cout, tasks_);
            std::cout << "\n";
        }
    }

    const SimulationMetrics& metrics() const {
        return metrics_;
    }

private:
    void reset() {
        active_jobs_.clear();
        job_counter_.assign(tasks_.size(), 0);

        metrics_.init_from_tasks(tasks_, horizon_);
    }

    void print_taskset(std::ostream& os) const {
        os << "=== Task set ===\n";
        os << std::left
           << std::setw(6)  << "Idx"
           << std::setw(6)  << "ID"
           << std::setw(6)  << "Prio"
           << std::setw(6)  << "T"
           << std::setw(6)  << "D"
           << std::setw(6)  << "C"
           << std::setw(6)  << "O"
           << "\n";

        os << std::string(6 + 6 + 6 + 6 + 6 + 6 + 6, '-') << "\n";

        for (size_t i = 0; i < tasks_.size(); ++i) {
            const auto& t = tasks_[i];

            os << std::left
               << std::setw(6) << i
               << std::setw(6) << t.id
               << std::setw(6) << t.priority
               << std::setw(6) << t.period
               << std::setw(6) << t.deadline
               << std::setw(6) << t.wcet
               << std::setw(6) << t.offset
               << "\n";
        }

        os << "\n";
    }

    void print_timeline_line(std::ostream& os, tick_t now, const Job& j) const {
        os << "t=" << std::setw(4) << now
           << "  RUN  task_id=" << std::setw(3) << j.task_id
           << "  job=" << std::setw(3) << j.job_index
           << "  rem->" << std::setw(3) << j.remaining_time;

        if (j.finish_time.has_value() && *j.finish_time == now + 1) {
            tick_t late = *j.finish_time - j.abs_deadline;

            os << "  FINISH@" << *j.finish_time
               << "  dl=" << j.abs_deadline
               << "  late=" << (late > 0 ? late : 0);
        }

        os << "\n";
    }

private:
    std::vector<Task> tasks_;

    // Contiene solo i job ancora attivi, cioè rilasciati ma non completati.
    //
    // Nella versione precedente il simulatore teneva tutti i job in jobs_.
    // Questo faceva crescere il vettore durante tutta la simulazione e
    // aumentava il costo della selezione del job a ogni tick.
    std::vector<Job> active_jobs_;

    tick_t horizon_;

    std::vector<int> job_counter_;

    SimulationMetrics metrics_;
};

} // namespace rt