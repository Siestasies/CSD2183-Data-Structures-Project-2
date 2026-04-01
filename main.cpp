#include <algorithm>
#include <cmath>
#include <fstream>
#include <iomanip>
#include <iostream>
#include <queue>
#include <sstream>
#include <string>
#include <unordered_map>
#include <unordered_set>
#include <vector>
#include <chrono>

struct Point {
    double x, y;
    Point() : x(0), y(0) {}
    Point(double x, double y) : x(x), y(y) {}
};

// each vertex is a node in a circular doubly linked list (one list per ring)
struct Vertex {
    Point pos;
    Vertex* prev = nullptr;
    Vertex* next = nullptr;
    int ring_id = -1;
    int id = -1;
    int last_query = 0;   // epoch counter for grid query dedup
    bool removed = false;
};

// represents one possible segment collapse (A->B->C->D becomes A->E->D)
// we store B because that's the first of the two vertices being replaced
struct Collapse {
    Vertex* b_vert = nullptr;
    double cost = 0.0;       // areal displacement
    int version = 0;         // for detecting stale PQ entries
};

// min heap by cost, break ties by vertex id
struct CmpCollapse {
    bool operator()(const Collapse& a, const Collapse& b) const {
        if (a.cost != b.cost) return a.cost > b.cost;
        return a.b_vert->id > b.b_vert->id;
    }
};

// --- geometry helpers ---

double cross2d(double ax, double ay, double bx, double by) {
    return ax * by - ay * bx;
}

double triangle_area(const Point& a, const Point& b, const Point& c) {
    return 0.5 * std::fabs(cross2d(b.x - a.x, b.y - a.y, c.x - a.x, c.y - a.y));
}

// shoelace formula on a circular linked list
double ring_area(Vertex* start) {
    double sum = 0.0;
    Vertex* v = start;
    do {
        sum += cross2d(v->pos.x, v->pos.y, v->next->pos.x, v->next->pos.y);
        v = v->next;
    } while (v != start);
    return sum * 0.5;
}

// strict proper intersection (no touching/collinear)
bool edges_cross(const Point& a, const Point& b, const Point& c, const Point& d) {
    double d1 = cross2d(d.x-c.x, d.y-c.y, a.x-c.x, a.y-c.y);
    double d2 = cross2d(d.x-c.x, d.y-c.y, b.x-c.x, b.y-c.y);
    double d3 = cross2d(b.x-a.x, b.y-a.y, c.x-a.x, c.y-a.y);
    double d4 = cross2d(b.x-a.x, b.y-a.y, d.x-a.x, d.y-a.y);
    return ((d1 > 0 && d2 < 0) || (d1 < 0 && d2 > 0)) &&
           ((d3 > 0 && d4 < 0) || (d3 < 0 && d4 > 0));
}

// find where two infinite lines intersect (false if parallel)
bool line_intersect(const Point& p1, const Point& p2,
                    const Point& p3, const Point& p4, Point& out) {
    double a1 = p2.y - p1.y, b1 = p1.x - p2.x;
    double c1 = a1 * p1.x + b1 * p1.y;
    double a2 = p4.y - p3.y, b2 = p3.x - p4.x;
    double c2 = a2 * p3.x + b2 * p3.y;
    double det = a1 * b2 - a2 * b1;
    if (std::fabs(det) < 1e-15) return false;
    out.x = (c1 * b2 - c2 * b1) / det;
    out.y = (a1 * c2 - a2 * c1) / det;
    return true;
}

// checks if point P is (nearly) on segment S1-S2
bool point_on_seg(const Point& p, const Point& s1, const Point& s2) {
    double dx = s2.x - s1.x, dy = s2.y - s1.y;
    double len_sq = dx*dx + dy*dy;
    if (len_sq < 1e-30) return false;
    double t = ((p.x - s1.x)*dx + (p.y - s1.y)*dy) / len_sq;
    if (t < -1e-9 || t > 1.0 + 1e-9) return false;
    double cx = s1.x + t*dx - p.x;
    double cy = s1.y + t*dy - p.y;
    return (cx*cx + cy*cy) < 1e-6 * len_sq;
}

// --- Kronenfeld APSC: find the Steiner point E for collapsing A->B->C->D to A->E->D ---
// returns false if degenerate
bool find_steiner(Vertex* A, Vertex* B, Vertex* C, Vertex* D,
                  Point& E, double& cost) {
    double ax = A->pos.x, ay = A->pos.y;
    double bx = B->pos.x, by = B->pos.y;
    double cx = C->pos.x, cy = C->pos.y;
    double dx = D->pos.x, dy = D->pos.y;

    // constraint line E* (parallel to AD) from Kronenfeld eq. 1b
    double a_coef = dy - ay;
    double b_coef = ax - dx;
    double c_coef = -by*ax + (ay - cy)*bx + (by - dy)*cx + cy*dx;

    if (std::fabs(a_coef) < 1e-15 && std::fabs(b_coef) < 1e-15) return false;

    // get two points on E* so we can intersect it with AB or CD
    Point ep1, ep2;
    if (std::fabs(a_coef) >= std::fabs(b_coef))
        ep1 = Point(-c_coef / a_coef, 0);
    else
        ep1 = Point(0, -c_coef / b_coef);
    ep2 = Point(ep1.x + (dx - ax), ep1.y + (dy - ay));

    Point e_from_ab, e_from_cd;
    bool got_ab = line_intersect(ep1, ep2, A->pos, B->pos, e_from_ab);
    bool got_cd = line_intersect(ep1, ep2, C->pos, D->pos, e_from_cd);

    // Kronenfeld Fig. 4: choose which line to place E on
    double dist_b = cross2d(dx-ax, dy-ay, bx-ax, by-ay);
    double dist_c = cross2d(dx-ax, dy-ay, cx-ax, cy-ay);
    double d_ad = dx*ay - ax*dy;
    double estar_side = c_coef - d_ad;

    bool use_ab;
    if ((dist_b > 0) != (dist_c > 0)) {
        use_ab = ((dist_b > 0) == (estar_side > 0));
    } else {
        use_ab = (std::fabs(dist_b) <= std::fabs(dist_c));
    }

    bool ok;
    if (use_ab) {
        ok = got_ab; E = e_from_ab;
        if (!ok) { ok = got_cd; E = e_from_cd; }
    } else {
        ok = got_cd; E = e_from_cd;
        if (!ok) { ok = got_ab; E = e_from_ab; }
    }
    if (!ok) return false;

    cost = triangle_area(A->pos, B->pos, E)
         + triangle_area(B->pos, C->pos, E)
         + triangle_area(C->pos, D->pos, E);
    return true;
}

// --- simple grid spatial index so intersection checks aren't O(n) ---

struct Grid {
    double ox, oy, cw, ch;
    int nx, ny;
    std::vector<std::vector<Vertex*>> cells;

    void build(double x1, double y1, double x2, double y2, int n_verts) {
        ox = x1 - 1; oy = y1 - 1;
        int side = std::max(1, std::min(256, (int)std::sqrt((double)n_verts)));
        nx = ny = side;
        cw = (x2 - x1 + 2) / nx;
        ch = (y2 - y1 + 2) / ny;
        cells.resize(nx * ny);
    }

    // get the range of grid cells that a bounding box overlaps
    void bbox_cells(double x1, double y1, double x2, double y2,
                    int& cx1, int& cy1, int& cx2, int& cy2) const {
        cx1 = std::max(0,    (int)((std::min(x1,x2) - ox) / cw));
        cy1 = std::max(0,    (int)((std::min(y1,y2) - oy) / ch));
        cx2 = std::min(nx-1, (int)((std::max(x1,x2) - ox) / cw));
        cy2 = std::min(ny-1, (int)((std::max(y1,y2) - oy) / ch));
    }

    void add(Vertex* v) {
        int cx1, cy1, cx2, cy2;
        bbox_cells(v->pos.x, v->pos.y, v->next->pos.x, v->next->pos.y,
                   cx1, cy1, cx2, cy2);
        for (int r = cy1; r <= cy2; r++)
            for (int c = cx1; c <= cx2; c++)
                cells[r*nx + c].push_back(v);
    }

    void remove(Vertex* v) {
        int cx1, cy1, cx2, cy2;
        bbox_cells(v->pos.x, v->pos.y, v->next->pos.x, v->next->pos.y,
                   cx1, cy1, cx2, cy2);
        for (int r = cy1; r <= cy2; r++)
            for (int c = cx1; c <= cx2; c++) {
                auto& cell = cells[r*nx + c];
                cell.erase(std::remove(cell.begin(), cell.end(), v), cell.end());
            }
    }

    // monotonic counter for dedup without hashing
    mutable int query_epoch = 0;

    // find all edge-start vertices near a bounding box (deduped)
    void query(double x1, double y1, double x2, double y2,
               std::vector<Vertex*>& out) {
        int cx1, cy1, cx2, cy2;
        bbox_cells(x1, y1, x2, y2, cx1, cy1, cx2, cy2);
        query_epoch++;
        for (int r = cy1; r <= cy2; r++)
            for (int c = cx1; c <= cx2; c++)
                for (auto* v : cells[r*nx + c])
                    if (v->last_query != query_epoch) {
                        v->last_query = query_epoch;
                        out.push_back(v);
                    }
    }
};

// --- globals ---

std::vector<Vertex*> all_vertices;
std::vector<Vertex*> ring_heads;
std::vector<int> ring_sizes;
std::vector<int> vert_version; // bumped when a neighbor changes, to invalidate stale PQ entries
int next_id = 0;
int total_verts = 0;
Grid grid;
std::priority_queue<Collapse, std::vector<Collapse>, CmpCollapse> pq;

Vertex* new_vertex(double x, double y, int ring) {
    Vertex* v = new Vertex();
    v->pos = Point(x, y);
    v->ring_id = ring;
    v->id = next_id++;
    all_vertices.push_back(v);
    vert_version.push_back(0);
    return v;
}

Vertex* build_ring(const std::vector<Point>& pts, int ring) {
    if (pts.empty()) return nullptr;
    Vertex* head = new_vertex(pts[0].x, pts[0].y, ring);
    Vertex* prev = head;
    for (size_t i = 1; i < pts.size(); i++) {
        Vertex* v = new_vertex(pts[i].x, pts[i].y, ring);
        prev->next = v;
        v->prev = prev;
        prev = v;
    }
    prev->next = head;
    head->prev = prev;
    return head;
}

// --- topology check: would this collapse create any intersections? ---

bool collapse_invalid(Vertex* A, Vertex* B, Vertex* C, Vertex* D, const Point& E) {
    // flat array instead of unordered_set — avoids heap allocation on every call
    Vertex* skip[5] = {A, B, C, D, A->prev};
    int skip_n = A->prev ? 5 : 4;
    auto in_skip = [&](Vertex* v) {
        for (int i = 0; i < skip_n; i++) if (skip[i] == v) return true;
        return false;
    };

    // bounding box of the two new edges A->E and E->D
    double lo_x = std::min({A->pos.x, E.x, D->pos.x});
    double lo_y = std::min({A->pos.y, E.y, D->pos.y});
    double hi_x = std::max({A->pos.x, E.x, D->pos.x});
    double hi_y = std::max({A->pos.y, E.y, D->pos.y});

    std::vector<Vertex*> nearby;
    grid.query(lo_x, lo_y, hi_x, hi_y, nearby);

    for (auto* v : nearby) {
        if (v->removed || v->next->removed) continue;
        if (in_skip(v) || in_skip(v->next)) continue;

        // do the new edges cross this existing edge?
        if (edges_cross(A->pos, E, v->pos, v->next->pos)) return true;
        if (edges_cross(E, D->pos, v->pos, v->next->pos)) return true;
    }

    // also make sure no nearby vertex lands exactly on a new edge (would mean rings touching)
    for (auto* v : nearby) {
        if (v->removed) continue;
        for (Vertex* cand : {v, v->next}) {
            if (in_skip(cand) || cand->removed) continue;
            if (point_on_seg(cand->pos, A->pos, E) ||
                point_on_seg(cand->pos, E, D->pos))
                return true;
        }
    }
    return false;
}

// --- compute the actual symmetric-difference displacement (for output reporting) ---
// the new path A->E->D might cross the old path A->B->C->D, so we split at
// crossing points and sum up the absolute area of each piece

double compute_real_displacement(const Point& A, const Point& B, const Point& C,
                                 const Point& D, const Point& E) {
    Point new_pts[3] = {A, E, D};
    Point old_pts[4] = {A, B, C, D};

    // find where new edges cross old edges
    struct Crossing { Point pt; int ni, oi; double nt, ot; };
    std::vector<Crossing> crossings;

    for (int ni = 0; ni < 2; ni++) {
        for (int oi = 0; oi < 3; oi++) {
            double dx1 = new_pts[ni+1].x - new_pts[ni].x;
            double dy1 = new_pts[ni+1].y - new_pts[ni].y;
            double dx2 = old_pts[oi+1].x - old_pts[oi].x;
            double dy2 = old_pts[oi+1].y - old_pts[oi].y;
            double det = dx1*dy2 - dy1*dx2;
            if (std::fabs(det) < 1e-15) continue;
            double dx3 = old_pts[oi].x - new_pts[ni].x;
            double dy3 = old_pts[oi].y - new_pts[ni].y;
            double t = (dx3*dy2 - dy3*dx2) / det;
            double u = (dx3*dy1 - dy3*dx1) / det;
            if (t > 1e-9 && t < 1-1e-9 && u > 1e-9 && u < 1-1e-9) {
                Crossing cr;
                cr.pt = Point(new_pts[ni].x + t*dx1, new_pts[ni].y + t*dy1);
                cr.ni = ni; cr.oi = oi; cr.nt = t; cr.ot = u;
                crossings.push_back(cr);
            }
        }
    }

    // no crossings: displacement is just the area of polygon ABCDE
    if (crossings.empty()) {
        Point poly[5] = {A, B, C, D, E};
        double area = 0;
        for (int i = 0; i < 5; i++) {
            int j = (i+1) % 5;
            area += cross2d(poly[i].x, poly[i].y, poly[j].x, poly[j].y);
        }
        return std::fabs(area * 0.5);
    }

    // with crossings: build both paths with crossing points inserted, then
    // walk matching segments and sum absolute areas of each sub-polygon
    auto build_path = [&](Point* seg, int n_seg, bool use_ni) {
        std::vector<Point> path;
        for (int i = 0; i < n_seg; i++) {
            path.push_back(seg[i]);
            std::vector<std::pair<double, Point>> here;
            for (auto& cr : crossings)
                if ((use_ni ? cr.ni : cr.oi) == i)
                    here.push_back({use_ni ? cr.nt : cr.ot, cr.pt});
            std::sort(here.begin(), here.end(),
                      [](auto& a, auto& b){ return a.first < b.first; });
            for (auto& [t, pt] : here) path.push_back(pt);
        }
        path.push_back(seg[n_seg]);
        return path;
    };

    auto old_path = build_path(old_pts, 3, false);
    auto new_path = build_path(new_pts, 2, true);

    // find indices of crossing points in each path
    auto find_splits = [&](const std::vector<Point>& path) {
        std::vector<int> splits = {0};
        for (int i = 1; i < (int)path.size()-1; i++)
            for (auto& cr : crossings) {
                double dx = path[i].x - cr.pt.x, dy = path[i].y - cr.pt.y;
                if (dx*dx + dy*dy < 1e-18) { splits.push_back(i); break; }
            }
        splits.push_back((int)path.size()-1);
        return splits;
    };

    auto old_sp = find_splits(old_path);
    auto new_sp = find_splits(new_path);

    // if something went wrong matching, just fall back to triangle fan
    if (old_sp.size() != new_sp.size())
        return triangle_area(A,B,E) + triangle_area(B,C,E) + triangle_area(C,D,E);

    double total = 0;
    for (int s = 0; s < (int)old_sp.size()-1; s++) {
        std::vector<Point> poly;
        for (int i = old_sp[s]; i <= old_sp[s+1]; i++) poly.push_back(old_path[i]);
        for (int i = new_sp[s+1]-1; i > new_sp[s]; i--) poly.push_back(new_path[i]);
        if (poly.size() < 3) continue;
        double area = 0;
        for (int i = 0; i < (int)poly.size(); i++) {
            int j = (i+1) % (int)poly.size();
            area += cross2d(poly[i].x, poly[i].y, poly[j].x, poly[j].y);
        }
        total += std::fabs(area * 0.5);
    }
    return total;
}

// --- priority queue management ---

void push_collapse(Vertex* B) {
    if (B->removed) return;
    if (ring_sizes[B->ring_id] <= 3) return;

    Point E;
    double cost;
    if (!find_steiner(B->prev, B, B->next, B->next->next, E, cost)) return;

    // use exact symmetric-difference displacement for more accurate PQ ordering
    // (triangle-fan cost overcounts when new/old paths cross, distorting greedy order)
    double real_cost = compute_real_displacement(
        B->prev->pos, B->pos, B->next->pos, B->next->next->pos, E);
    pq.push({B, real_cost, vert_version[B->id]});
}

// try to collapse B. returns actual displacement if successful, -1 if blocked
double do_collapse(Vertex* B) {
    Vertex* A = B->prev;
    Vertex* C = B->next;
    Vertex* D = C->next;
    int ring = B->ring_id;
    if (ring_sizes[ring] <= 3) return -1;

    Point E;
    double cost;
    if (!find_steiner(A, B, C, D, E, cost)) return -1;
    if (collapse_invalid(A, B, C, D, E)) return -1;

    double real_disp = compute_real_displacement(A->pos, B->pos, C->pos, D->pos, E);

    // update the grid (remove old edges, will add new ones after relinking)
    grid.remove(A); grid.remove(B); grid.remove(C);

    // create new vertex and splice it in
    Vertex* ve = new_vertex(E.x, E.y, ring);
    A->next = ve; ve->prev = A;
    ve->next = D; D->prev = ve;
    B->removed = true;
    C->removed = true;

    grid.add(A); grid.add(ve);

    if (ring_heads[ring] == B || ring_heads[ring] == C)
        ring_heads[ring] = ve;

    ring_sizes[ring]--;
    total_verts--;

    // bump versions so stale PQ entries get skipped
    vert_version[A->id]++;
    vert_version[ve->id] = 0;
    vert_version[D->id]++;
    if (A->prev) vert_version[A->prev->id]++;
    if (D->next) vert_version[D->next->id]++;

    // enqueue new candidates for all affected vertices
    if (ring_sizes[ring] > 3) {
        push_collapse(A->prev);
        push_collapse(A);
        push_collapse(ve);
        push_collapse(D);
        push_collapse(D->next);
    }
    return real_disp;
}

// --- look-ahead: evaluate how a collapse affects its neighborhood ---
// temporarily splice a Steiner point into the ring, compute the resulting
// neighbor collapse costs, then revert. returns own displacement + weighted
// average of neighbor costs, or 1e30 if invalid.

double lookahead_score(Vertex* B) {
    Vertex* A = B->prev;
    Vertex* C = B->next;
    Vertex* D = C->next;
    int ring = B->ring_id;
    if (ring_sizes[ring] <= 3) return 1e30;

    Point E;
    double cost;
    if (!find_steiner(A, B, C, D, E, cost)) return 1e30;
    // skip topology check in look-ahead (expensive grid query, and
    // do_collapse will check before committing anyway)

    // own actual displacement
    double own_disp = compute_real_displacement(A->pos, B->pos, C->pos, D->pos, E);

    // temporarily splice in E to evaluate neighbor collapse costs
    Vertex temp_e;
    temp_e.pos = E;
    temp_e.ring_id = ring;
    temp_e.id = -1;
    temp_e.removed = false;
    temp_e.prev = A;
    temp_e.next = D;

    Vertex* old_a_next = A->next;
    Vertex* old_d_prev = D->prev;
    A->next = &temp_e;
    D->prev = &temp_e;

    int new_ring_size = ring_sizes[ring] - 1;
    double neighbor_sum = 0;
    int neighbor_count = 0;

    if (new_ring_size > 3) {
        Point ne; double nc;
        // five affected neighbor collapse sequences
        if (A->prev && A->prev->prev &&
            find_steiner(A->prev->prev, A->prev, A, &temp_e, ne, nc))
            { neighbor_sum += nc; neighbor_count++; }
        if (A->prev &&
            find_steiner(A->prev, A, &temp_e, D, ne, nc))
            { neighbor_sum += nc; neighbor_count++; }
        if (D->next &&
            find_steiner(A, &temp_e, D, D->next, ne, nc))
            { neighbor_sum += nc; neighbor_count++; }
        if (D->next && D->next->next &&
            find_steiner(&temp_e, D, D->next, D->next->next, ne, nc))
            { neighbor_sum += nc; neighbor_count++; }
        if (D->next && D->next->next && D->next->next->next &&
            find_steiner(D, D->next, D->next->next, D->next->next->next, ne, nc))
            { neighbor_sum += nc; neighbor_count++; }
    }

    A->next = old_a_next;
    D->prev = old_d_prev;

    double avg_neighbor = (neighbor_count > 0) ? (neighbor_sum / neighbor_count) : 0;
    return own_disp + 0.15 * avg_neighbor;
}

// --- CSV parsing ---

struct RawVertex { int ring, vid; double x, y; };

std::vector<RawVertex> read_csv(const std::string& path) {
    std::vector<RawVertex> out;
    std::ifstream f(path);
    if (!f.is_open()) {
        std::cerr << "can't open " << path << std::endl;
        std::exit(1);
    }
    std::string line;
    std::getline(f, line); // skip header
    while (std::getline(f, line)) {
        if (line.empty()) continue;
        if (line.back() == '\r') line.pop_back();
        if (line.empty()) continue;
        RawVertex rv;
        char sep;
        std::istringstream ss(line);
        ss >> rv.ring >> sep >> rv.vid >> sep >> rv.x >> sep >> rv.y;
        out.push_back(rv);
    }
    return out;
}

// reads VmPeak from /proc/self/status and returns peak virtual memory usage in kB,
// or -1 if unavailable
long get_peak_memory_kb() {
    std::ifstream f("/proc/self/status");
    std::string line;
    while (std::getline(f, line)) {
        if (line.rfind("VmPeak:", 0) == 0) {
            long kb;
            std::sscanf(line.c_str(), "VmPeak: %ld kB", &kb);
            return kb;
        }
    }
    return -1;
}

// --- main ---

int main(int argc, char* argv[]) {
    if (argc != 3) {
        std::cerr << "Usage: " << argv[0] << " <input.csv> <target_verts>" << std::endl;
        return 1;
    }

    int target = std::atoi(argv[2]);
    using clock = std::chrono::high_resolution_clock;
    using ms = std::chrono::duration<double, std::milli>;

    auto t_parse_start = clock::now();
    auto raw = read_csv(argv[1]);
    auto t_parse_end = clock::now();

    // pre-allocate: each collapse creates 1 Steiner point, so at most 2x vertices
    all_vertices.reserve(raw.size() * 2);
    vert_version.reserve(raw.size() * 2);

    // group vertices by ring
    std::unordered_map<int, std::vector<Point>> rings;
    int max_ring = -1;
    for (auto& rv : raw) {
        rings[rv.ring].push_back(Point(rv.x, rv.y));
        max_ring = std::max(max_ring, rv.ring);
    }

    // build circular linked lists
    ring_heads.resize(max_ring + 1, nullptr);
    ring_sizes.resize(max_ring + 1, 0);
    total_verts = 0;
    for (int r = 0; r <= max_ring; r++) {
        if (rings.find(r) == rings.end()) continue;
        ring_heads[r] = build_ring(rings[r], r);
        ring_sizes[r] = (int)rings[r].size();
        total_verts += ring_sizes[r];
    }

    // compute input area + bounding box for the spatial grid
    double input_area = 0;
    double lo_x = 1e18, lo_y = 1e18, hi_x = -1e18, hi_y = -1e18;
    for (int r = 0; r <= max_ring; r++) {
        if (!ring_heads[r]) continue;
        input_area += ring_area(ring_heads[r]);
        Vertex* v = ring_heads[r];
        do {
            lo_x = std::min(lo_x, v->pos.x); lo_y = std::min(lo_y, v->pos.y);
            hi_x = std::max(hi_x, v->pos.x); hi_y = std::max(hi_y, v->pos.y);
            v = v->next;
        } while (v != ring_heads[r]);
    }

    // set up the spatial grid and fill it with all edges
    grid.build(lo_x, lo_y, hi_x, hi_y, total_verts);
    for (int r = 0; r <= max_ring; r++) {
        if (!ring_heads[r] || ring_sizes[r] < 3) continue;
        Vertex* v = ring_heads[r];
        do { grid.add(v); v = v->next; } while (v != ring_heads[r]);
    }

    // seed the priority queue with every possible initial collapse
    for (int r = 0; r <= max_ring; r++) {
        if (!ring_heads[r] || ring_sizes[r] <= 3) continue;
        Vertex* v = ring_heads[r];
        do { push_collapse(v); v = v->next; } while (v != ring_heads[r]);
    }

    auto t_setup_end = clock::now();

    // collapse with depth-1 look-ahead over top-k candidates
    // look-ahead is applied when remaining vertices are manageable;
    // for very large inputs early on, use plain greedy to stay fast
    const int LOOKAHEAD_THRESHOLD = 200;
    double total_disp = 0;
    int stale_count = 0;       // track stale PQ pops for periodic rebuild
    int collapse_count = 0;
    auto t_simplify_start = clock::now();
    while (total_verts > target && !pq.empty()) {
        // adaptive look-ahead K: more candidates when fewer vertices remain
        bool use_lookahead = (total_verts <= LOOKAHEAD_THRESHOLD);
        int lookahead_k = (total_verts <= 50) ? 8 : 5;

        if (!use_lookahead) {
            // plain greedy: just take the cheapest valid candidate
            auto top = pq.top(); pq.pop();
            if (top.b_vert->removed) { stale_count++; continue; }
            if (top.version != vert_version[top.b_vert->id]) { stale_count++; continue; }
            if (ring_sizes[top.b_vert->ring_id] <= 3) continue;
            double d = do_collapse(top.b_vert);
            if (d >= 0) { total_disp += d; collapse_count++; }
        } else {
            // look-ahead: collect top-k, evaluate neighborhood, pick best
            std::vector<Collapse> candidates;
            candidates.reserve(lookahead_k);
            while ((int)candidates.size() < lookahead_k && !pq.empty()) {
                auto top = pq.top(); pq.pop();
                if (top.b_vert->removed) { stale_count++; continue; }
                if (top.version != vert_version[top.b_vert->id]) { stale_count++; continue; }
                if (ring_sizes[top.b_vert->ring_id] <= 3) continue;
                candidates.push_back(top);
            }
            if (candidates.empty()) break;

            // among similar-cost candidates, use look-ahead to pick best
            int best_idx = 0;
            if (candidates.size() > 1) {
                double cheapest = candidates[0].cost;
                double threshold = cheapest * 1.5 + 1e-12;
                if (candidates[1].cost <= threshold) {
                    double best_score = lookahead_score(candidates[0].b_vert);
                    for (int i = 1; i < (int)candidates.size(); i++) {
                        if (candidates[i].cost > threshold) break;
                        double score = lookahead_score(candidates[i].b_vert);
                        // early termination: skip remaining if already much worse
                        if (score < best_score) {
                            best_score = score;
                            best_idx = i;
                        }
                    }
                }
            }

            for (int i = 0; i < (int)candidates.size(); i++) {
                if (i != best_idx) pq.push(candidates[i]);
            }

            double d = do_collapse(candidates[best_idx].b_vert);
            if (d >= 0) { total_disp += d; collapse_count++; }
        }

        // periodic PQ rebuild: if stale entries dominate, rebuild to reduce wasted pops
        if (stale_count > 500 && stale_count > collapse_count * 3) {
            std::vector<Collapse> valid;
            valid.reserve(pq.size() / 2);
            while (!pq.empty()) {
                auto top = pq.top(); pq.pop();
                if (!top.b_vert->removed &&
                    top.version == vert_version[top.b_vert->id] &&
                    ring_sizes[top.b_vert->ring_id] > 3)
                    valid.push_back(top);
            }
            for (auto& c : valid) pq.push(c);
            stale_count = 0;
            collapse_count = 0;
        }
    }
    auto t_simplify_end = clock::now();

    // compute output area
    double output_area = 0;
    for (int r = 0; r <= max_ring; r++)
        if (ring_heads[r]) output_area += ring_area(ring_heads[r]);

    // print simplified polygon
    std::cout << "ring_id,vertex_id,x,y" << std::endl;
    for (int r = 0; r <= max_ring; r++) {
        if (!ring_heads[r]) continue;
        Vertex* v = ring_heads[r];
        int vid = 0;
        do {
            std::cout << r << "," << vid << ","
                      << std::setprecision(15) << v->pos.x << ","
                      << std::setprecision(15) << v->pos.y << std::endl;
            vid++;
            v = v->next;
        } while (v != ring_heads[r]);
    }
    std::cout << std::endl;
    std::cout << "Total signed area in input: "
              << std::scientific << std::setprecision(6) << input_area << std::endl;
    std::cout << "Total signed area in output: "
              << std::scientific << std::setprecision(6) << output_area << std::endl;
    std::cout << "Total areal displacement: "
              << std::scientific << std::setprecision(6) << total_disp << std::endl;

    std::cerr << std::fixed << std::setprecision(3);
    std::cerr << "Time - CSV parsing:    " << ms(t_parse_end    - t_parse_start).count()    << " ms" << std::endl;
    std::cerr << "Time - Data setup:     " << ms(t_setup_end    - t_parse_end).count()      << " ms" << std::endl;
    std::cerr << "Time - Simplification: " << ms(t_simplify_end - t_simplify_start).count() << " ms" << std::endl;
    std::cerr << "Time - Total:          " << ms(t_simplify_end - t_parse_start).count()    << " ms" << std::endl;
    std::cerr << "Peak memory:           " << get_peak_memory_kb()                          << " kB" << std::endl;

    for (auto* v : all_vertices) delete v;
    return 0;
}
