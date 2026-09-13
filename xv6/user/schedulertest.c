#include "kernel/types.h"
#include "kernel/stat.h"
#include "user/user.h"

// Ticks of genuine CPU demand each CPU-bound child should hold, regardless
// of host speed. 1+4+8+16 = 29 ticks is enough to fully descend from queue 0
// to queue 3; 60 leaves it sitting in queue 3 long enough to also catch a
// priority boost (every 48 ticks) if the run overlaps one.
#define CPU_TARGET_TICKS 60

#define NCPU_BOUND 3   // purely CPU-bound children
#define NIO_BOUND  3   // children that alternate CPU work with sleep()
#define IO_ITERS   5   // how many work/sleep cycles each I/O child does
#define IO_SLEEP_TICKS 5

static void
cpu_bound_child(int id)
{
  int pid = getpid();
  int start = uptime();
  printf("cpu[%d] pid=%d start tick=%d\n", id, pid, start);
  // Spin against uptime() rather than a fixed iteration count, so this
  // child's CPU demand reliably outlasts several MLFQ slice lengths no
  // matter how fast the host executes the inner loop.
  while (uptime() - start < CPU_TARGET_TICKS) {
    volatile long x = 0;
    for (long i = 0; i < 200000; i++)
      x++;
  }
  printf("cpu[%d] pid=%d done  tick=%d\n", id, pid, uptime());
  exit(0);
}

static void
io_bound_child(int id)
{
  int pid = getpid();
  printf("io[%d] pid=%d start tick=%d\n", id, pid, uptime());
  for (int k = 0; k < IO_ITERS; k++) {
    // A little CPU work, then voluntarily give up the CPU.
    // This should re-enqueue at the SAME queue, not demote.
    volatile long x = 0;
    for (long i = 0; i < 300000; i++)
      x++;
    pause(IO_SLEEP_TICKS);
  }
  printf("io[%d] pid=%d done  tick=%d\n", id, pid, uptime());
  exit(0);
}

int
main(int argc, char *argv[])
{
  int pid;
  int start = uptime();
  printf("schedulertest: start tick=%d\n", start);

  for (int i = 0; i < NCPU_BOUND; i++) {
    pid = fork();
    if (pid < 0) {
      printf("schedulertest: fork failed\n");
      exit(1);
    }
    if (pid == 0)
      cpu_bound_child(i);
  }

  for (int i = 0; i < NIO_BOUND; i++) {
    pid = fork();
    if (pid < 0) {
      printf("schedulertest: fork failed\n");
      exit(1);
    }
    if (pid == 0)
      io_bound_child(i);
  }

  for (int i = 0; i < NCPU_BOUND + NIO_BOUND; i++)
    wait(0);

  printf("schedulertest: all children done, elapsed=%d ticks\n", uptime() - start);
  exit(0);
}