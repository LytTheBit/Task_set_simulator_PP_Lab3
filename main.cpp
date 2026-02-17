// main.cpp
// Created by Francesco on 17/02/2026.
//
// Entry point: test del simulatore real-time FPP con output esteso.

#include <iostream>
#include <vector>

#include "include/simulator.hpp"

int main() {
    using namespace rt;

    std::vector<Task> tasks = {
        {0, 5, 5, 2, 1, 0},  // id, T, D, C, priority, offset
        {1, 7, 7, 3, 0, 0}
    };

    tick_t horizon = 30;

    Simulator sim(tasks, horizon);

    bool debug_timeline = true;   // metti false se vuoi output più corto
    bool print_input = true;

    sim.run(debug_timeline, print_input);

    return 0;
}
