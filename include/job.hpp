// job.hpp
// Created by Francesco on 17/02/2026.
//
// Definizione della struttura dati per rappresentare un job, ovvero un'istanza di un task.
#pragma once

#include <cstdint>
#include <optional>
#include <stdexcept>
#include <string>

#include "task.hpp"

namespace rt {

struct Job {
    id_t   task_id = 0;

    // Indice del job all'interno del task (0,1,2,...) utile per debug/log.
    std::int32_t job_index = 0;

    tick_t release_time = 0;
    tick_t abs_deadline = 0;

    tick_t remaining_time = 0;

    // start_time = primo tick in cui il job viene eseguito.
    std::optional<tick_t> start_time;

    // finish_time = tick in cui il job completa (convenzione: tempo di completamento).
    std::optional<tick_t> finish_time;

    // Costruzione a partire dal Task e dal tempo di rilascio.
    static Job from_task(const Task& task, tick_t release_t, std::int32_t index) {
        task.validate();
        if (release_t < 0) {
            throw std::invalid_argument("release_t must be >= 0");
        }
        Job j;
        j.task_id = task.id;
        j.job_index = index;
        j.release_time = release_t;
        j.abs_deadline = release_t + task.deadline;
        j.remaining_time = task.wcet;
        return j;
    }

    bool is_completed() const {
        return remaining_time == 0;
    }

    // Job pronto: già rilasciato e non completato.
    bool is_ready(tick_t now) const {
        return (now >= release_time) && (remaining_time > 0);
    }

    // Esegue 1 tick di CPU sul job al tempo "now".
    // Con tick-based simulation, tipicamente chiami execute_one_tick(t) e poi incrementi t.
    void execute_one_tick(tick_t now) {
        if (!is_ready(now)) {
            throw std::logic_error("execute_one_tick called on non-ready job");
        }

        if (!start_time.has_value()) {
            start_time = now;
        }

        // Consuma 1 tick di computazione
        remaining_time -= 1;

        if (remaining_time < 0) {
            throw std::logic_error("remaining_time became negative (bug in simulation)");
        }

        // Convenzione: se finisce durante il tick [now, now+1),
        // segnamo finish_time = now + 1.
        if (remaining_time == 0) {
            finish_time = now + 1;
        }
    }

    // Response time = finish_time - release_time (se completato).
    std::optional<tick_t> response_time() const {
        if (!finish_time.has_value()) return std::nullopt;
        return *finish_time - release_time;
    }

    std::string to_string() const {
        return "Job{task_id=" + std::to_string(task_id) +
               ", idx=" + std::to_string(job_index) +
               ", r=" + std::to_string(release_time) +
               ", dl=" + std::to_string(abs_deadline) +
               ", rem=" + std::to_string(remaining_time) +
               ", start=" + (start_time ? std::to_string(*start_time) : "n/a") +
               ", finish=" + (finish_time ? std::to_string(*finish_time) : "n/a") +
               "}";
    }
};

} // namespace rt
