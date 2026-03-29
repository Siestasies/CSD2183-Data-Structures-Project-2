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

// shoelace formula on a circular linked list
double cross2d(double ax, double ay, double bx, double by) {
    return ax * by - ay * bx;
}

double ring_area(Vertex* start) {
    double sum = 0.0;
    Vertex* v = start;
    do {
        sum += cross2d(v->pos.x, v->pos.y, v->next->pos.x, v->next->pos.y);
        v = v->next;
    } while (v != start);
    return sum * 0.5;
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
