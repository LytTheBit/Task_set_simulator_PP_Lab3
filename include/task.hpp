// Task.hpp
// Created by Francesco on 17/02/2026.
//
// Definizione della struttura dati per rappresentare un task periodico.

#pragma once

#include <cstdint>
#include <stdexcept>
#include <string>

namespace rt {

using tick_t = std::int64_t;   // 1 tick = 1 ms fittizio (intero)
using id_t   = std::int32_t;

// Convenzione: priorità numerica più PICCOLA => priorità più ALTA.
using prio_t = std::int32_t;

struct Task {
    id_t   id = 0;
    tick_t period = 0;      // T
    tick_t deadline = 0;    // D (relativa)
    tick_t wcet = 0;        // C
    prio_t priority = 0;    // P (minore = più alta)
    tick_t offset = 0;      // O (default 0)

    // Validazione semplice (utile anche nel parsing).
    void validate() const {
        if (id < 0) {
            throw std::invalid_argument("Task.id must be >= 0");
        }
        if (period <= 0) {
            throw std::invalid_argument("Task.period must be > 0");
        }
        if (deadline <= 0) {
            throw std::invalid_argument("Task.deadline must be > 0");
        }
        if (wcet <= 0) {
            throw std::invalid_argument("Task.wcet must be > 0");
        }
        if (offset < 0) {
            throw std::invalid_argument("Task.offset must be >= 0");
        }
        // Scelte conservative per iniziare: D <= T e C <= T.
        // Se in seguito vuoi supportare D > T o C > T, rivediamo le semantiche.
        if (deadline > period) {
            throw std::invalid_argument("Task.deadline must be <= Task.period (for now)");
        }
        if (wcet > period) {
            throw std::invalid_argument("Task.wcet must be <= Task.period (for now)");
        }
    }

    // Il task rilascia un job al tick t?
    // Nota: con offset, il primo rilascio è t == offset.
    bool releases_at(tick_t t) const {
        if (t < offset) return false;
        return ((t - offset) % period) == 0;
    }

    std::string to_string() const {
        return "Task{id=" + std::to_string(id) +
               ", T=" + std::to_string(period) +
               ", D=" + std::to_string(deadline) +
               ", C=" + std::to_string(wcet) +
               ", P=" + std::to_string(priority) +
               ", O=" + std::to_string(offset) + "}";
    }
};

} // namespace rt
