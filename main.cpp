#include <algorithm>
#include <cmath>
#include <fstream>
#include <iomanip>
#include <iostream>
#include <sstream>
#include <string>
#include <unordered_map>
#include <vector>

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
    bool removed = false;
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

// --- globals ---

std::vector<Vertex*> all_vertices;
std::vector<Vertex*> ring_heads;
std::vector<int> ring_sizes;
int next_id = 0;
int total_verts = 0;

Vertex* new_vertex(double x, double y, int ring) {
    Vertex* v = new Vertex();
    v->pos = Point(x, y);
    v->ring_id = ring;
    v->id = next_id++;
    all_vertices.push_back(v);
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

// --- main ---

int main(int argc, char* argv[]) {
    if (argc != 3) {
        std::cerr << "Usage: " << argv[0] << " <input.csv> <target_verts>" << std::endl;
        return 1;
    }

    int target = std::atoi(argv[2]);
    auto raw = read_csv(argv[1]);

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

    // compute input area
    double input_area = 0;
    for (int r = 0; r <= max_ring; r++) {
        if (!ring_heads[r]) continue;
        input_area += ring_area(ring_heads[r]);
    }

    std::cout << "Total vertices: " << total_verts << std::endl;
    std::cout << "Target: " << target << std::endl;
    std::cout << "Input area: " << std::scientific << std::setprecision(6) << input_area << std::endl;

    // print output
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

    for (auto* v : all_vertices) delete v;
    return 0;
}
