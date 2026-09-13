# MLFQ & Cross-Scheduler Comparison Report

## 2.3.1 Implementation Summary

- **Makefile / SCHEDULER macro:** Added `SCHEDULER` macro flags in the Makefile to easily switch between MLFQ, FIFO, and RR by conditionally defining `-DMLFQ` and `-DFIFO` via `CFLAGS`.
- **struct proc changes:** Added `queue` and `ticks` under `#ifdef MLFQ` to track the process priority and time slice spent in that queue. Also added `ctime`, `rtime`, `iotime`, `etime`, and `first_run_time` for scheduler metrics calculation.
- **allocproc() changes:** Initialized `p->queue = 0` and `p->ticks = 0`. Also initialized all timing metrics for Turnaround/Waiting/Response time calculations on allocation.
- **queue selection/preemption logic:** Handled in `scheduler()` by replacing the linear scan with a priority-based multi-level queue array (`mlfq[4][NPROC]`). Queues are guarded by `mlfq_lock`. Highest priority queues are checked first and `p->state == RUNNABLE` is verified before executing. Preemption from a higher queue arriving is checked inside `yield()`.
- **time-slice handling:** On a timer interrupt (in `trap.c`), `yield()` is called. In `yield()`, the process's `ticks` in its current queue are checked against the limits (1, 4, 8, 16). If it exceeds the quantum, it gets preempted, moves down a queue level, and its ticks reset to 0.
- **voluntary yield handling:** If a process gives up CPU via `sleep()`, its ticks are reset. When woken up via `wakeup()`, it's placed back at the tail of the same queue (`p->queue` remains unchanged), successfully maintaining its priority level.
- **priority boosting:** Implemented in `trap.c` inside `clockintr()` by calling `boost_priority()` every 48 ticks. The boost safely locks `mlfq_lock` and the individual process locks to avoid deadlocks, resetting all processes to `queue 0` and `ticks 0`.
- **procdump changes:** Extends `procdump()` (`ctrl+p`) by printing `p->queue` and `p->ticks` alongside the normal debugging data.

## 2.3.2 MLFQ Analysis

The python script `plot_mlfq.py` successfully parses `MLFQ_PLOT` traces dumped from the scheduler logic to generate a scatter plot / timeline of processes.
The plot clearly showcases CPU-bound processes migrating from queue 0 downwards into queue 3 due to exhausting their time-slices, while I/O bound tasks remain in higher priority queues. Every 48 ticks, you can visibly observe all processes instantly migrating back up to Queue 0, demonstrating the anti-starvation priority boost correctly executing.

## 2.3.3 Comparison Results

| Metric | FIFO | Round Robin (RR) | MLFQ |
| --- | --- | --- | --- |
| Average Turnaround Time | High | Moderate | Low |
| Average Waiting Time | High | High | Low |
| Average Response Time | High | Low | Low |

### Trade-offs Observed
MLFQ consistently yields lower average Turnaround and Waiting times than FIFO because it actively preempts processes, preventing long-running CPU-bound tasks from monopolizing the CPU (the Convoy effect seen in FIFO). Furthermore, MLFQ has a superior response time to FIFO due to its queueing mechanics (Queue 0 time slice of just 1 tick ensures I/O bounds are executed immediately). While Round Robin guarantees a fair response time regardless of task length, its average waiting time fluctuates drastically depending on the quantum size; if the quantum is too small, context switching dominates, but if too large, it degrades into FIFO. MLFQ optimally solves this by utilizing progressively larger quanta to balance responsive I/O tasks with CPU-bound efficiency.
