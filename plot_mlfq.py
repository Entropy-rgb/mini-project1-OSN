#!/usr/bin/env python3
"""
Parses MLFQ_PLOT lines from a captured xv6 console log and produces
the queue-vs-time scatter plot required by the mini-project report.

Expected log line format (adjust the regex below if yours differs):
    MLFQ_PLOT <tick> <pid> <queue>

Usage:
    python3 plot_mlfq.py run.log your_iiit_username
"""

import re
import sys
import matplotlib.pyplot as plt

LINE_RE = re.compile(r"MLFQ_PLOT\s+(\d+)\s+(\d+)\s+(\d+)")


def parse_log(path):
    """Returns dict: pid -> (list of ticks, list of queues)."""
    data = {}
    with open(path, "r", errors="ignore") as f:
        for line in f:
            m = LINE_RE.search(line)
            if not m:
                continue
            tick, pid, queue = map(int, m.groups())
            data.setdefault(pid, ([], []))
            data[pid][0].append(tick)
            data[pid][1].append(queue)
    return data


def plot(data, watermark, out_path="mlfq_timeline.png"):
    if not data:
        print("No MLFQ_PLOT lines found — check the log path and line format.")
        sys.exit(1)

    fig, ax = plt.subplots(figsize=(11, 6))

    # Stable color per pid, sorted so the legend is predictable.
    pids = sorted(data.keys())
    cmap = plt.get_cmap("tab10")
    colors = {pid: cmap(i % 10) for i, pid in enumerate(pids)}

    for pid in pids:
        ticks, queues = data[pid]
        ax.scatter(ticks, queues, s=14, color=colors[pid], label=f"pid {pid}")

    ax.set_xlabel("Ticks elapsed since scheduler start")
    ax.set_ylabel("MLFQ queue level")
    ax.set_yticks([0, 1, 2, 3])
    ax.set_ylim(-0.5, 3.5)
    ax.set_title("MLFQ: process queue level over time")
    ax.legend(loc="upper right", fontsize=8)
    ax.grid(True, alpha=0.3)

    # Watermark, per the assignment's requirement.
    fig.text(
        0.5, 0.5, watermark,
        fontsize=40, color="gray", alpha=0.15,
        ha="center", va="center", rotation=30,
    )

    fig.tight_layout()
    fig.savefig(out_path, dpi=150)
    print(f"Saved plot to {out_path}")


if __name__ == "__main__":
    if len(sys.argv) != 3:
        print(f"Usage: python3 {sys.argv[0]} <log_file> <iiit_username>")
        sys.exit(1)

    log_path, username = sys.argv[1], sys.argv[2]
    data = parse_log(log_path)
    plot(data, watermark=username)