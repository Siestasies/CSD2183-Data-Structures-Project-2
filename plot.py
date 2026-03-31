import sys
import csv
import matplotlib.pyplot as plt
from collections import defaultdict

def read_input_csv(path):
    rings = defaultdict(list)
    with open(path) as f:
        reader = csv.reader(f)
        next(reader)  # skip header
        for row in reader:
            ring_id, vid, x, y = int(row[0]), int(row[1]), float(row[2]), float(row[3])
            rings[ring_id].append((x, y))
    return rings

def read_output_txt(path):
    rings = defaultdict(list)
    with open(path) as f:
        for line in f:
            line = line.strip()
            if not line or line.startswith("ring_id") or line.startswith("Total") or line.startswith("Time") or line.startswith("Peak"):
                continue
            parts = line.split(",")
            if len(parts) == 4:
                ring_id, vid, x, y = int(parts[0]), int(parts[1]), float(parts[2]), float(parts[3])
                rings[ring_id].append((x, y))
    return rings

def close_ring(pts):
    if pts and pts[0] != pts[-1]:
        pts = pts + [pts[0]]
    return pts

def plot_rings(ax, rings, color, label, linewidth=1.0):
    first = True
    for ring_id in sorted(rings):
        pts = close_ring(rings[ring_id])
        xs = [p[0] for p in pts]
        ys = [p[1] for p in pts]
        ax.plot(xs, ys, color=color, linewidth=linewidth, label=label if first else None)
        first = False

if __name__ == "__main__":
    if len(sys.argv) != 3:
        print(f"Usage: python {sys.argv[0]} <input.csv> <output.txt>")
        sys.exit(1)

    input_rings = read_input_csv(sys.argv[1])
    output_rings = read_output_txt(sys.argv[2])

    input_verts = sum(len(v) for v in input_rings.values())
    output_verts = sum(len(v) for v in output_rings.values())

    fig, (ax1, ax2) = plt.subplots(1, 2, figsize=(14, 6))

    # before
    plot_rings(ax1, input_rings, "blue", "Input")
    ax1.set_title(f"Before ({input_verts} vertices)")
    ax1.set_aspect("equal")
    ax1.legend()

    # after
    plot_rings(ax2, output_rings, "red", "Output")
    ax2.set_title(f"After ({output_verts} vertices)")
    ax2.set_aspect("equal")
    ax2.legend()

    fig.suptitle(f"{sys.argv[1]} → {sys.argv[2]}")
    plt.tight_layout()
    plt.savefig("plot.png", dpi=150)
    print("Saved plot.png")
    plt.show()
