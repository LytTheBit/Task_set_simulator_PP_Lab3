cmake_minimum_required(VERSION 4.1)
project(Task_set_simulator_PP_Lab3)

set(CMAKE_CXX_STANDARD 20)

add_executable(Task_set_simulator_PP_Lab3 main.cpp
        include/task.hpp
        include/job.hpp
        include/scheduler.hpp
        include/simulator.hpp
        include/metrics.hpp
        include/time_utils.hpp
        include/csv_export.hpp
        include/batch_runner.hpp
        include/taskset_generator.hpp)

target_compile_definitions(Task_set_simulator_PP_Lab3 PRIVATE PROJECT_ROOT_DIR="${CMAKE_SOURCE_DIR}")