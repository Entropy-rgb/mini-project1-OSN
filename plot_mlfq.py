import matplotlib.pyplot as plt
import sys

# Read from xv6 output file or stdin
if len(sys.argv) > 1:
    with open(sys.argv[1], 'r') as f:
        lines = f.readlines()
else:
    lines = sys.stdin.readlines()

data = {}
for line in lines:
    if line.startswith("MLFQ_PLOT"):
        parts = line.strip().split()
        if len(parts) == 4:
            tick = int(parts[1])
            pid = int(parts[2])
            queue = int(parts[3])
            if pid not in data:
                data[pid] = {'x': [], 'y': []}
            data[pid]['x'].append(tick)
            data[pid]['y'].append(queue)

plt.figure(figsize=(10, 6))
for pid, coords in data.items():
    plt.scatter(coords['x'], coords['y'], label=f"PID {pid}", s=10)
    plt.plot(coords['x'], coords['y'], alpha=0.3)

plt.yticks([0, 1, 2, 3], ['Queue 0', 'Queue 1', 'Queue 2', 'Queue 3'])
plt.xlabel("Time (Ticks)")
plt.ylabel("Queue ID")
plt.title("MLFQ Scheduler - Process Queues over Time")
plt.legend()
plt.grid(True, linestyle='--', alpha=0.6)

plt.text(0.5, 0.5, 'somesh.kamad', transform=plt.gca().transAxes,
         fontsize=40, color='gray', alpha=0.2,
         ha='center', va='center', rotation=30)

plt.savefig("mlfq_plot.png", dpi=300, bbox_inches='tight')
print("Saved plot to mlfq_plot.png")
