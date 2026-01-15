#include "router.h"

#include <algorithm>
#include <chrono>
#include <cstdlib>
#include <fstream>
#include <iostream>
#include <limits>
#include <queue>
#include <string>
#include <vector>

using namespace std;

void GlobalRouter::parseInput(const string& filename) {
    ifstream infile(filename);
    if (!infile.is_open()) {
        cerr << "Error: Unable to open file " << filename << endl;
        exit(1);
    }

    string temp;
    infile >> temp >> gridWidth >> gridHeight;
    infile >> temp >> hCapacity >> vCapacity;
    infile >> temp >> totalNets;

    nets.resize(totalNets);
    for (int i = 0; i < totalNets; ++i) {
        infile >> temp >> nets[i].name >> nets[i].numPins;
        nets[i].id = i;
        nets[i].pins.resize(nets[i].numPins);
        for (int j = 0; j < nets[i].numPins; ++j) {
            string pinName;
            infile >> temp >> pinName >> nets[i].pins[j].x >> nets[i].pins[j].y;
        }
    }

    infile.close();
}

void GlobalRouter::runRouting(double timeLimitSeconds) {
    initGrid();
    initRouteLShape();

    int overflow = totalOverflow();
    auto start = chrono::steady_clock::now();

    while (overflow > 0) {
        auto now = chrono::steady_clock::now();
        double elapsed = chrono::duration<double>(now - start).count();
        if (elapsed >= timeLimitSeconds) {
            break;
        }

        vector<vector<char>> hOverflow(gridHeight, vector<char>(max(0, gridWidth - 1), 0));
        vector<vector<char>> vOverflow(max(0, gridHeight - 1), vector<char>(gridWidth, 0));
        markOverflowEdges(hOverflow, vOverflow);

        vector<pair<int, int>> rerouteNets;
        for (auto& net : nets) {
            int overflowEdges = countOverflowEdges(net.routePath, hOverflow, vOverflow);
            if (overflowEdges > 0) {
                ripUp(net);
                rerouteNets.push_back({overflowEdges, net.id});
            }
        }

        if (rerouteNets.empty()) {
            break;
        }

        updateHistoryPenalty(hOverflow, vOverflow);
        sort(rerouteNets.begin(), rerouteNets.end(),
             [](const pair<int, int>& a, const pair<int, int>& b) {
                 return a.first > b.first;
             });
        for (const auto& item : rerouteNets) {
            routeAStar(nets[item.second]);
        }

        overflow = totalOverflow();
    }
}

void GlobalRouter::writeOutput(const string& filename) {
    ofstream out(filename);
    if (!out.is_open()) {
        cerr << "Error: Unable to open output file " << filename << endl;
        return;
    }

    int totalWirelength = 0;
    for (const auto& net : nets) {
        if (net.routePath.size() >= 2) {
            totalWirelength += static_cast<int>(net.routePath.size()) - 1;
        }
    }

    out << "Wirelength " << totalWirelength << "\n";
    for (const auto& net : nets) {
        out << "Net " << net.name << "\n";
        auto segments = buildSegments(net.routePath);
        for (const auto& seg : segments) {
            out << "Segment " << seg.first.x << " " << seg.first.y << " "
                << seg.second.x << " " << seg.second.y << "\n";
        }
    }
}

bool GlobalRouter::inBounds(int x, int y) const {
    return x >= 0 && x < gridWidth && y >= 0 && y < gridHeight;
}

void GlobalRouter::initGrid() {
    hUsage.assign(gridHeight, vector<int>(max(0, gridWidth - 1), 0));
    vUsage.assign(max(0, gridHeight - 1), vector<int>(gridWidth, 0));
    hHistory.assign(gridHeight, vector<int>(max(0, gridWidth - 1), 0));
    vHistory.assign(max(0, gridHeight - 1), vector<int>(gridWidth, 0));
}

void GlobalRouter::initRouteLShape() {
    for (auto& net : nets) {
        routeLShape(net);
    }
}

void GlobalRouter::routeLShape(Net& net) {
    if (net.pins.size() < 2) {
        return;
    }
    Point start{net.pins[0].x, net.pins[0].y};
    Point end{net.pins[1].x, net.pins[1].y};

    vector<Point> pathXFirst = buildManhattanPath(start, end, true);
    vector<Point> pathYFirst = buildManhattanPath(start, end, false);

    double costXFirst = pathCost(pathXFirst);
    double costYFirst = pathCost(pathYFirst);

    if (costXFirst <= costYFirst) {
        net.routePath = pathXFirst;
    } else {
        net.routePath = pathYFirst;
    }
    addPathUsage(net.routePath, 1);
}

vector<Point> GlobalRouter::buildManhattanPath(Point start, Point end, bool xFirst) const {
    vector<Point> path;
    path.push_back(start);

    if (xFirst) {
        int x = start.x;
        int y = start.y;
        int dx = (end.x >= x) ? 1 : -1;
        while (x != end.x) {
            x += dx;
            path.push_back(Point{x, y});
        }
        int dy = (end.y >= y) ? 1 : -1;
        while (y != end.y) {
            y += dy;
            path.push_back(Point{x, y});
        }
    } else {
        int x = start.x;
        int y = start.y;
        int dy = (end.y >= y) ? 1 : -1;
        while (y != end.y) {
            y += dy;
            path.push_back(Point{x, y});
        }
        int dx = (end.x >= x) ? 1 : -1;
        while (x != end.x) {
            x += dx;
            path.push_back(Point{x, y});
        }
    }
    return path;
}

double GlobalRouter::pathCost(const vector<Point>& path) const {
    double cost = 0.0;
    if (path.size() < 2) {
        return cost;
    }
    for (size_t i = 1; i < path.size(); ++i) {
        cost += edgeCost(path[i - 1], path[i], true);
    }
    return cost;
}

int GlobalRouter::totalOverflow() const {
    int overflow = 0;
    for (int y = 0; y < gridHeight; ++y) {
        for (int x = 0; x < gridWidth - 1; ++x) {
            int over = hUsage[y][x] - hCapacity;
            if (over > 0) {
                overflow += over;
            }
        }
    }
    for (int y = 0; y < gridHeight - 1; ++y) {
        for (int x = 0; x < gridWidth; ++x) {
            int over = vUsage[y][x] - vCapacity;
            if (over > 0) {
                overflow += over;
            }
        }
    }
    return overflow;
}

void GlobalRouter::markOverflowEdges(vector<vector<char>>& hOverflow,
                                     vector<vector<char>>& vOverflow) const {
    for (int y = 0; y < gridHeight; ++y) {
        for (int x = 0; x < gridWidth - 1; ++x) {
            if (hUsage[y][x] > hCapacity) {
                hOverflow[y][x] = 1;
            }
        }
    }
    for (int y = 0; y < gridHeight - 1; ++y) {
        for (int x = 0; x < gridWidth; ++x) {
            if (vUsage[y][x] > vCapacity) {
                vOverflow[y][x] = 1;
            }
        }
    }
}

bool GlobalRouter::usesOverflowEdge(const vector<Point>& path,
                                    const vector<vector<char>>& hOverflow,
                                    const vector<vector<char>>& vOverflow) const {
    return countOverflowEdges(path, hOverflow, vOverflow) > 0;
}

int GlobalRouter::countOverflowEdges(const vector<Point>& path,
                                     const vector<vector<char>>& hOverflow,
                                     const vector<vector<char>>& vOverflow) const {
    if (path.size() < 2) {
        return 0;
    }
    int count = 0;
    for (size_t i = 1; i < path.size(); ++i) {
        const Point& a = path[i - 1];
        const Point& b = path[i];
        if (a.x != b.x) {
            int x = min(a.x, b.x);
            int y = a.y;
            if (hOverflow[y][x]) {
                count += 1;
            }
        } else {
            int x = a.x;
            int y = min(a.y, b.y);
            if (vOverflow[y][x]) {
                count += 1;
            }
        }
    }
    return count;
}

void GlobalRouter::ripUp(Net& net) {
    addPathUsage(net.routePath, -1);
    net.routePath.clear();
}

void GlobalRouter::updateHistoryPenalty(const vector<vector<char>>& hOverflow,
                                        const vector<vector<char>>& vOverflow) {
    for (int y = 0; y < gridHeight; ++y) {
        for (int x = 0; x < gridWidth - 1; ++x) {
            if (hOverflow[y][x]) {
                hHistory[y][x] += 1;
            }
        }
    }
    for (int y = 0; y < gridHeight - 1; ++y) {
        for (int x = 0; x < gridWidth; ++x) {
            if (vOverflow[y][x]) {
                vHistory[y][x] += 1;
            }
        }
    }
}

void GlobalRouter::routeAStar(Net& net) {
    if (net.pins.size() < 2) {
        return;
    }
    Point start{net.pins[0].x, net.pins[0].y};
    Point goal{net.pins[1].x, net.pins[1].y};

    int totalVertices = gridWidth * gridHeight;
    vector<double> gCost(totalVertices, numeric_limits<double>::infinity());
    vector<int> parent(totalVertices, -1);
    vector<char> closed(totalVertices, 0);

    auto index = [this](int x, int y) { return y * gridWidth + x; };

    struct Node {
        int x, y;
        double f;
    };
    struct NodeCmp {
        bool operator()(const Node& a, const Node& b) const { return a.f > b.f; }
    };
    priority_queue<Node, vector<Node>, NodeCmp> open;

    int startIdx = index(start.x, start.y);
    gCost[startIdx] = 0.0;
    open.push(Node{start.x, start.y, heuristic(start, goal)});

    const int dirs[4][2] = {{1,0},{-1,0},{0,1},{0,-1}};

    while (!open.empty()) {
        Node current = open.top();
        open.pop();
        int curIdx = index(current.x, current.y);
        if (closed[curIdx]) {
            continue;
        }
        closed[curIdx] = 1;

        if (current.x == goal.x && current.y == goal.y) {
            break;
        }

        for (const auto& d : dirs) {
            int nx = current.x + d[0];
            int ny = current.y + d[1];
            if (!inBounds(nx, ny)) {
                continue;
            }
            double stepCost = edgeCost(Point{current.x, current.y}, Point{nx, ny}, true);
            int nIdx = index(nx, ny);
            double tentative = gCost[curIdx] + stepCost;
            if (tentative < gCost[nIdx]) {
                gCost[nIdx] = tentative;
                parent[nIdx] = curIdx;
                double fScore = tentative + heuristic(Point{nx, ny}, goal);
                open.push(Node{nx, ny, fScore});
            }
        }
    }

    vector<Point> path = reconstructPath(parent, start, goal);
    if (path.empty()) {
        path = buildManhattanPath(start, goal, true);
    }
    net.routePath = path;
    addPathUsage(net.routePath, 1);
}

vector<Point> GlobalRouter::reconstructPath(const vector<int>& parent, Point start, Point goal) const {
    auto index = [this](int x, int y) { return y * gridWidth + x; };
    int goalIdx = index(goal.x, goal.y);
    int startIdx = index(start.x, start.y);
    if (parent[goalIdx] == -1 && goalIdx != startIdx) {
        return {};
    }

    vector<Point> path;
    int cur = goalIdx;
    while (cur != -1) {
        int x = cur % gridWidth;
        int y = cur / gridWidth;
        path.push_back(Point{x, y});
        if (cur == startIdx) {
            break;
        }
        cur = parent[cur];
    }
    reverse(path.begin(), path.end());
    return path;
}

double GlobalRouter::heuristic(Point a, Point b) const {
    return static_cast<double>(abs(a.x - b.x) + abs(a.y - b.y));
}

double GlobalRouter::edgeCost(Point a, Point b, bool addNet) const {
    int usage = 0;
    int capacity = 0;
    int history = 0;
    if (a.x != b.x) {
        int x = min(a.x, b.x);
        int y = a.y;
        usage = hUsage[y][x];
        capacity = hCapacity;
        history = hHistory[y][x];
    } else {
        int x = a.x;
        int y = min(a.y, b.y);
        usage = vUsage[y][x];
        capacity = vCapacity;
        history = vHistory[y][x];
    }

    if (addNet) {
        usage += 1;
    }
    double congestion = capacity > 0 ? static_cast<double>(usage) / capacity : usage * 10.0;
    return 1.0 + congestionWeight * congestion + historyWeight * history;
}

void GlobalRouter::addPathUsage(const vector<Point>& path, int delta) {
    if (path.size() < 2) {
        return;
    }
    for (size_t i = 1; i < path.size(); ++i) {
        const Point& a = path[i - 1];
        const Point& b = path[i];
        if (a.x != b.x) {
            int x = min(a.x, b.x);
            int y = a.y;
            hUsage[y][x] += delta;
        } else {
            int x = a.x;
            int y = min(a.y, b.y);
            vUsage[y][x] += delta;
        }
    }
}

vector<pair<Point, Point>> GlobalRouter::buildSegments(const vector<Point>& path) const {
    vector<pair<Point, Point>> segments;
    if (path.size() < 2) {
        return segments;
    }

    Point segStart = path[0];
    Point prev = path[0];
    int dirX = path[1].x - path[0].x;
    int dirY = path[1].y - path[0].y;

    for (size_t i = 1; i < path.size(); ++i) {
        Point cur = path[i];
        int curDirX = cur.x - prev.x;
        int curDirY = cur.y - prev.y;
        if (curDirX != dirX || curDirY != dirY) {
            segments.push_back({segStart, prev});
            segStart = prev;
            dirX = curDirX;
            dirY = curDirY;
        }
        prev = cur;
    }
    segments.push_back({segStart, prev});
    return segments;
}
