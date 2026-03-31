# Area- and Topology-Preserving Polygon Simplification

APSC (Area-Preserving Segment Collapse) algorithm based on the paper by Kronenfeld et al. (2020) for CSD2183 Data Structures Assignment 2.

The goal is to reduce the number of vertices in a polygon while keeping the total area exactly the same and making sure no edges cross or overlap. It handles polygons with holes too.

## How to Build

You just need a C++17 compiler (g++). No external libraries needed.

```bash
make
```

This gives you a `simplify` executable in the project root.

## How to Run

```bash
./simplify <input_file.csv> <target_vertices>
```

- `input_file.csv` - a CSV file with columns `ring_id,vertex_id,x,y`
- `target_vertices` - how many vertices you want in the simplified result

The output goes to stdout: a CSV of the simplified polygon, followed by three lines showing the original area, simplified area and the areal displacement between them.

## How It Works

The basic idea is to repeatedly collapse pairs of adjacent vertices into a single Steiner point, always picking the collapse that causes the least distortion.

- **Circular doubly-linked lists** store each polygon ring, so inserting and removing vertices during collapses is O(1).
- **A min-heap (priority queue)** keeps track of all possible collapses sorted by how much area they'd displace. Stale entries are handled with version counters on each vertex rather than deleting from the heap.
- **Steiner point placement** when collapsing vertices B and C (with neighbors A and D), the replacement point E is placed along a constraint line that preserves the area exactly. The algorithm picks the best placement following the rules from the Kronenfeld paper.
- **Grid-based spatial index** speeds up intersection checks so the program can handle large inputs (100K+ vertices) without grinding to a halt.
- **Topology checks** run before every collapse to make sure no new edges would cross existing ones, and no vertex ends up sitting on an edge.

## Testing

The `test_cases/` folder has a bunch of test inputs and their expected outputs. Simple polygons, polygons with holes, and larger lake outlines. All tests pass with area preserved exactly (within floating-point tolerance) and no topology violations.
