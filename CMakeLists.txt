cmake_minimum_required(VERSION 4.1)
project(Task_set_simulator_PP_Lab3)

set(CMAKE_CXX_STANDARD 20)

add_executable(Task_set_simulator_PP_Lab3 main.cpp
        include/task.hpp
        include/job.hpp
        include/scheduler.hpp
        include/simulator.hpp)
