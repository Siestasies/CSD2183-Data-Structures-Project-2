#!/usr/bin/env python3
"""Visualize original vs simplified polygon from APSC output."""

import sys
import csv
import matplotlib.pyplot as plt
from collections import defaultdict


def read_polygon(filepath):
    """Read ring_id,vertex_id,x,y CSV (stops at non-CSV lines like area summary)."""
    rings = defaultdict(list)
    with open(filepath, "r") as f:
        reader = csv.reader(f)
        next(reader)  # skip header
        for row in reader:
            if len(row) < 4:
                break
            try:
                rid = int(row[0])
                x, y = float(row[2]), float(row[3])
                rings[rid].append((x, y))
            except ValueError:
                break
    return rings


def plot_rings(ax, rings, color, label_prefix, linewidth=1.5, alpha=1.0):
    """Plot each ring as a closed polygon."""
    first = True
    for rid in sorted(rings):
        pts = rings[rid]
        xs = [p[0] for p in pts] + [pts[0][0]]
        ys = [p[1] for p in pts] + [pts[0][1]]
        label = f"{label_prefix} (ring {rid})" if first else None
        ax.plot(xs, ys, color=color, linewidth=linewidth, alpha=alpha, label=label)
        first = False


def main():
    if len(sys.argv) < 3:
        print(f"Usage: {sys.argv[0]} <input.csv> <output.txt> [save_path.png]")
        sys.exit(1)

    input_file = sys.argv[1]
    output_file = sys.argv[2]
    save_path = sys.argv[3] if len(sys.argv) > 3 else None

    original = read_polygon(input_file)
    simplified = read_polygon(output_file)

    orig_verts = sum(len(v) for v in original.values())
    simp_verts = sum(len(v) for v in simplified.values())

    fig, axes = plt.subplots(1, 2, figsize=(14, 6))

    # Left: overlay
    plot_rings(axes[0], original, "#2196F3", "Original", linewidth=1.0, alpha=0.6)
    plot_rings(axes[0], simplified, "#F44336", "Simplified", linewidth=2.0, alpha=0.9)
    axes[0].set_title(f"Overlay ({orig_verts} → {simp_verts} vertices)")
    axes[0].set_aspect("equal")
    axes[0].legend(fontsize=9)
    axes[0].grid(True, alpha=0.3)

    # Right: simplified only
    plot_rings(axes[1], simplified, "#F44336", "Simplified", linewidth=2.0)
    # mark vertices
    for rid in sorted(simplified):
        pts = simplified[rid]
        xs = [p[0] for p in pts]
        ys = [p[1] for p in pts]
        axes[1].scatter(xs, ys, color="#D32F2F", s=20, zorder=5)
    axes[1].set_title(f"Simplified ({simp_verts} vertices)")
    axes[1].set_aspect("equal")
    axes[1].grid(True, alpha=0.3)

    fig.suptitle("APSC Polygon Simplification", fontsize=14, fontweight="bold")
    plt.tight_layout()

    if save_path:
        plt.savefig(save_path, dpi=150, bbox_inches="tight")
        print(f"Saved to {save_path}")
    else:
        plt.show()


if __name__ == "__main__":
    main()
