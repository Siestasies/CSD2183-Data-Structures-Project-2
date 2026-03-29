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

    int total_verts = 0;
    for (int r = 0; r <= max_ring; r++) {
        if (rings.find(r) == rings.end()) continue;
        std::cout << "Ring " << r << ": " << rings[r].size() << " vertices" << std::endl;
        total_verts += (int)rings[r].size();
    }
    std::cout << "Total vertices: " << total_verts << std::endl;
    std::cout << "Target: " << target << std::endl;

    return 0;
}
