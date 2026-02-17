// main.cpp
// Created by Francesco on 17/02/2026.
//
// Test minimale del simulatore FPP tick-based.

#include <iostream>
#include "simulator.hpp"

int main() {

    using namespace rt;

    std::vector<Task> tasks = {
        {0, 5, 5, 2, 1, 0},  // id, T, D, C, priority, offset
        {1, 7, 7, 3, 0, 0}   // task 1 ha priorità più alta (0 < 1)
    };

    for (auto& t : tasks) {
        t.validate();
    }

    Simulator sim(tasks, 30);
    sim.run(true);

    std::cout << "\nDeadline miss: " << sim.deadline_miss() << "\n";
    std::cout << "Utilization: " << sim.utilization() << "\n";

    return 0;
}
