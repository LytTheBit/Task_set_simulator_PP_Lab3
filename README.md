# Task Set Simulator - Parallel Programming Lab 3

This project implements a C++20 simulator for periodic real-time task sets executed on a single processor under a Fixed-Priority Preemptive scheduling policy.

The priority assignment follows a Rate-Monotonic strategy: tasks with shorter periods receive higher priority.

The project was developed for the Parallel Programming course and focuses on:

- real-time task-set simulation;
- fixed-priority preemptive scheduling;
- batch execution of many independent simulations;
- OpenMP parallelization;
- performance analysis and optimization.

---

## Project Overview

The simulator models a set of periodic tasks. Each task periodically releases jobs that must execute before their deadlines.

For each simulation tick, the simulator performs:

1. release of new jobs;
2. selection of the highest-priority ready job;
3. execution of one time unit;
4. update of global and per-task metrics.

The simulator collects metrics such as:

- execution time;
- CPU utilization;
- deadline misses;
- unfinished jobs;
- response time;
- lateness;
- OpenMP speedup;
- parallel efficiency.

---

## Real-Time Scheduling Model

Each task is defined as:

```text
Task = (T, D, C, P, O)
````

where:

| Symbol | Meaning                   |
| ------ | ------------------------- |
| `T`    | Period                    |
| `D`    | Relative deadline         |
| `C`    | Worst-case execution time |
| `P`    | Static priority           |
| `O`    | Offset                    |

Each released job has:

```text
absolute_deadline = release_time + relative_deadline
```

The scheduling policy is Fixed-Priority Preemptive.

At each tick, the scheduler selects the ready job with the highest priority. Lower numerical priority values correspond to higher priority.

---

## Parallelization Strategy

A single task-set simulation is inherently sequential because each tick depends on the previous one:

```text
state(t + 1) = f(state(t))
```

Therefore, the internal simulation loop is not parallelized.

Instead, the project uses batch-level parallelism:

```text
result_i = simulate(taskset_i)
```

Each task set is independent, so multiple task sets can be simulated concurrently using OpenMP.

The OpenMP parallelization is applied to the outer batch loop:

```cpp
#pragma omp parallel for schedule(runtime) num_threads(cfg.num_threads)
```

The real-time scheduling policy does not change. OpenMP scheduling only controls how independent task-set simulations are distributed among CPU threads.

---

## Main Optimizations

### 1. Active Job List

The original version stored all released jobs in a single vector, including jobs that had already completed.

This caused the scheduler to scan an increasingly large job list at every tick.

The optimized version uses:

```cpp
active_jobs_
```

This vector contains only jobs that are still active. Completed jobs are removed immediately.

This reduces the per-tick scheduling overhead without changing the RM/FPP scheduling semantics.

---

### 2. OpenMP Schedule Selection

The batch runner supports different OpenMP scheduling policies:

```cpp
static
dynamic
guided
```

These policies affect only the distribution of task-set simulations across OpenMP threads.

They do not affect the simulated real-time scheduling policy.

---

### 3. Dynamic Chunk Size Tuning

The final tuning experiment evaluates different chunk sizes for:

```cpp
schedule(dynamic, chunk_size)
```

In this project:

```text
1 OpenMP iteration = 1 task-set simulation
```

So:

| Configuration | Meaning                                     |
| ------------- | ------------------------------------------- |
| `dynamic,1`   | Each thread receives 1 task set at a time   |
| `dynamic,2`   | Each thread receives 2 task sets at a time  |
| `dynamic,4`   | Each thread receives 4 task sets at a time  |
| `dynamic,8`   | Each thread receives 8 task sets at a time  |
| `dynamic,16`  | Each thread receives 16 task sets at a time |

A smaller chunk size improves load balancing but increases scheduling overhead.
A larger chunk size reduces overhead but may increase load imbalance.

The best final configuration was:

```text
dynamic,2
```

---

## Experimental Setup

Main benchmark parameters:

| Parameter          |          Value |
| ------------------ | -------------: |
| Task sets          |     200 / 2000 |
| Tasks per task set |              8 |
| Period range       |      [10, 150] |
| Target utilization |           0.85 |
| Maximum horizon    |   200000 ticks |
| Threads tested     | 1, 2, 4, 8, 12 |
| Warmup runs        |              1 |
| Measured runs      |             10 |

The maximum horizon is used because the hyperperiod of randomly generated task sets can become very large.

```text
H = min(hyperperiod, Hmax)
```

---

## Results

### Original OpenMP Implementation

Initial OpenMP batch execution with `dynamic,1`:

| Threads | Mean Time (s) | Speedup | Efficiency |
| ------: | ------------: | ------: | ---------: |
|       1 |      2065.113 |  1.000x |      1.000 |
|       2 |      1781.120 |  1.159x |      0.580 |
|       4 |       620.114 |  3.330x |      0.833 |
|       8 |       370.455 |  5.575x |      0.697 |
|      12 |       349.156 |  5.915x |      0.493 |

---

### Active Job List Optimization

Comparison between the original implementation and the optimized active-job-list version:

| Threads | Original Time (s) | Optimized Time (s) | Improvement |
| ------: | ----------------: | -----------------: | ----------: |
|       1 |          2065.113 |              3.509 |      588.5x |
|       2 |          1781.120 |              1.784 |      998.4x |
|       4 |           620.114 |              0.999 |      620.7x |
|       8 |           370.455 |              0.690 |      536.9x |
|      12 |           349.156 |              0.596 |      585.8x |

The active job list is the most important optimization in the project.

It dramatically reduces execution time because completed jobs are no longer scanned by the scheduler at every tick.

---

### Larger Workload

After the simulator became much faster, the workload was increased from 200 task sets to 2000 task sets.

| Threads | 200 Task Sets Time (s) | 2000 Task Sets Time (s) |
| ------: | ---------------------: | ----------------------: |
|       1 |                  3.509 |                  35.505 |
|       2 |                  1.784 |                  24.344 |
|       4 |                  0.999 |                  10.421 |
|       8 |                  0.690 |                   6.208 |
|      12 |                  0.596 |                   5.771 |

Speedup comparison:

| Threads | 200 Task Sets Speedup | 2000 Task Sets Speedup |
| ------: | --------------------: | ---------------------: |
|       1 |                1.000x |                 1.000x |
|       2 |                1.967x |                 1.458x |
|       4 |                3.514x |                 3.407x |
|       8 |                5.085x |                 5.719x |
|      12 |                5.889x |                 6.152x |

With a larger workload, OpenMP overhead becomes less dominant and the final 12-thread speedup improves.

---

### Static vs Dynamic vs Guided

Using the 2000-task-set workload:

#### Execution Time

| Threads | Static Time (s) | Dynamic Time (s) | Guided Time (s) |
| ------: | --------------: | ---------------: | --------------: |
|       1 |          34.601 |           35.505 |          35.032 |
|       2 |          25.199 |           24.344 |          18.021 |
|       4 |          13.427 |           10.421 |          10.330 |
|       8 |           7.352 |            6.208 |           6.250 |
|      12 |           5.816 |            5.771 |           5.772 |

#### Speedup

| Threads | Static Speedup | Dynamic Speedup | Guided Speedup |
| ------: | -------------: | --------------: | -------------: |
|       1 |         1.000x |          1.000x |         1.000x |
|       2 |         1.373x |          1.458x |         1.944x |
|       4 |         2.577x |          3.407x |         3.391x |
|       8 |         4.706x |          5.719x |         5.605x |
|      12 |         5.949x |          6.152x |         6.069x |

Dynamic scheduling gives the best result at high thread counts, while guided performs very well at intermediate thread counts.

---

### Dynamic Chunk Size Tuning

#### Execution Time

| Chunk Size | 1T Time (s) | 2T Time (s) | 4T Time (s) | 8T Time (s) | 12T Time (s) |
| ---------: | ----------: | ----------: | ----------: | ----------: | -----------: |
|          1 |      34.581 |      18.466 |      10.438 |       6.216 |        5.679 |
|          2 |      35.688 |      18.258 |      10.323 |       6.134 |        5.578 |
|          4 |      34.451 |      17.706 |      10.161 |       6.069 |        5.586 |
|          8 |      34.282 |      17.700 |      10.124 |       6.631 |        5.991 |
|         16 |      34.473 |      17.618 |      10.087 |       6.111 |        5.653 |

#### Speedup

| Chunk Size | 1T Speedup | 2T Speedup | 4T Speedup | 8T Speedup | 12T Speedup |
| ---------: | ---------: | ---------: | ---------: | ---------: | ----------: |
|          1 |     1.000x |     1.873x |     3.313x |     5.563x |      6.089x |
|          2 |     1.000x |     1.955x |     3.457x |     5.818x |      6.398x |
|          4 |     1.000x |     1.946x |     3.391x |     5.677x |      6.168x |
|          8 |     1.000x |     1.937x |     3.386x |     5.170x |      5.723x |
|         16 |     1.000x |     1.957x |     3.418x |     5.641x |      6.098x |

The best configuration is:

```text
dynamic,2
```

with:

```text
12-thread time: 5.578 s
speedup: 6.398x
```

---

## Final Comparison

| Version / Experiment | Configuration                   | 1T Time (s) | 12T Time (s) | Speedup |
| -------------------- | ------------------------------- | ----------: | -----------: | ------: |
| Original             | 200 task sets, dynamic,1        |    2065.113 |      349.156 |  5.915x |
| Active Job List      | 200 task sets, dynamic,1        |       3.509 |        0.596 |  5.889x |
| Larger Workload      | 2000 task sets, dynamic,1       |      35.505 |        5.771 |  6.152x |
| Schedule Comparison  | 2000 task sets, best: dynamic   |      35.505 |        5.771 |  6.152x |
| Chunk Size Tuning    | 2000 task sets, best: dynamic,2 |      35.688 |        5.578 |  6.398x |

The largest performance improvement comes from the active job list optimization.

The best final configuration is:

```text
Active Job List + 2000 task sets + dynamic,2 + 12 threads
```

---

## Build Instructions

### Requirements

* C++20 compiler
* CMake
* OpenMP

### Build with CMake

```bash
mkdir build
cd build
cmake ..
cmake --build .
```

Depending on the compiler and platform, OpenMP support must be available and correctly detected by CMake.

---

## Run

From the build directory:

```bash
./Task_set_simulator_PP_Lab3
```

On Windows, the executable name may be:

```bash
Task_set_simulator_PP_Lab3.exe
```

The program generates benchmark results and writes CSV files in the `results/` directory.

---

## Output Files

The program can generate several CSV files, depending on the experiment:

| File                                  | Description                                          |
| ------------------------------------- | ---------------------------------------------------- |
| `scaling.csv`                         | Original OpenMP scaling results                      |
| `summary_seq.csv`                     | Summary metrics for the 1-thread baseline            |
| `summary_par.csv`                     | Summary metrics for the parallel run                 |
| `per_task_seq.csv`                    | Per-task metrics for the 1-thread baseline           |
| `per_task_par.csv`                    | Per-task metrics for the parallel run                |
| `scaling_optimized_all_schedules.csv` | Optimized scaling comparison                         |
| `scaling_dynamic_optimized.csv`       | Dynamic schedule scaling                             |
| `scaling_static_optimized.csv`        | Static schedule scaling                              |
| `scaling_guided_optimized.csv`        | Guided schedule scaling                              |
| `summary_dynamic_12t_optimized.csv`   | Summary metrics for optimized dynamic 12-thread run  |
| `per_task_dynamic_12t_optimized.csv`  | Per-task metrics for optimized dynamic 12-thread run |

---

## Repository Structure

A simplified structure of the project is:

```text
Task_set_simulator_PP_Lab3/
├── CMakeLists.txt
├── main.cpp
├── include/
│   ├── task.hpp
│   ├── job.hpp
│   ├── scheduler.hpp
│   ├── simulator.hpp
│   ├── metrics.hpp
│   ├── batch_runner.hpp
│   ├── taskset_generator.hpp
│   ├── csv_export.hpp
│   └── time_utils.hpp
└── results/
    ├── scaling.csv
    ├── summary_*.csv
    └── per_task_*.csv
```

---

## Key Implementation Details

### Simulator

The `Simulator` class performs the tick-based simulation.

It handles:

* job releases;
* job execution;
* preemption;
* active job tracking;
* metrics update.

The optimized version uses:

```cpp
std::vector<Job> active_jobs_;
```

instead of storing all historical jobs.

---

### Scheduler

The `SchedulerFPP` class selects the highest-priority ready job.

The scheduler does not implement EDF.
The scheduling policy remains Fixed-Priority Preemptive / Rate Monotonic.

---

### BatchRunner

The `BatchRunner` class executes multiple task-set simulations.

It supports:

* sequential batch execution;
* parallel OpenMP batch execution;
* runtime OpenMP schedule selection;
* CSV export after the parallel region.

---

## Notes

The distinction between real-time scheduling and OpenMP scheduling is important.

Real-time scheduling decides which job runs inside one simulation:

```text
FPP / RM
```

OpenMP scheduling decides how independent task-set simulations are assigned to CPU threads:

```text
static / dynamic / guided
```

Changing the OpenMP schedule does not change the real-time scheduling semantics.

---

## Conclusion

The project shows that batch-level OpenMP parallelism is effective for real-time scheduling simulations.

However, the most important improvement is algorithmic: using an active job list drastically reduces the cost of each simulation by preventing the scheduler from scanning completed jobs.

The final optimized version achieves:

```text
Best speedup: 6.398x
Best configuration: dynamic,2 with 12 threads
```

The project demonstrates the difference between:

* parallelizing independent work with OpenMP;
* optimizing the internal algorithmic cost of each simulation;
* tuning OpenMP scheduling parameters to reduce overhead.

```

