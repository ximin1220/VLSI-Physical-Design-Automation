#ifndef ROUTER_H
#define ROUTER_H

#include <string>
#include <vector>
#include "db.h"

class GlobalRouter {
public:
    int gridWidth = 0;
    int gridHeight = 0;
    int hCapacity = 0;
    int vCapacity = 0;
    int totalNets = 0;
    std::vector<Net> nets;

    std::vector<std::vector<int>> hUsage;
    std::vector<std::vector<int>> vUsage;
    std::vector<std::vector<int>> hHistory;
    std::vector<std::vector<int>> vHistory;

    double congestionWeight = 4.0;
    double historyWeight = 1.0;

    GlobalRouter() = default;

    void parseInput(const std::string& filename);
    void runRouting(double timeLimitSeconds = 440.0);
    void writeOutput(const std::string& filename);

private:
    bool inBounds(int x, int y) const;
    void initGrid();
    void initRouteLShape();
    void routeLShape(Net& net);
    std::vector<Point> buildManhattanPath(Point start, Point end, bool xFirst) const;
    double pathCost(const std::vector<Point>& path) const;
    int totalOverflow() const;
    void markOverflowEdges(std::vector<std::vector<char>>& hOverflow,
                           std::vector<std::vector<char>>& vOverflow) const;
    bool usesOverflowEdge(const std::vector<Point>& path,
                          const std::vector<std::vector<char>>& hOverflow,
                          const std::vector<std::vector<char>>& vOverflow) const;
    int countOverflowEdges(const std::vector<Point>& path,
                           const std::vector<std::vector<char>>& hOverflow,
                           const std::vector<std::vector<char>>& vOverflow) const;
    void ripUp(Net& net);
    void updateHistoryPenalty(const std::vector<std::vector<char>>& hOverflow,
                              const std::vector<std::vector<char>>& vOverflow);
    void routeAStar(Net& net);
    std::vector<Point> reconstructPath(const std::vector<int>& parent, Point start, Point goal) const;
    double heuristic(Point a, Point b) const;
    double edgeCost(Point a, Point b, bool addNet) const;
    void addPathUsage(const std::vector<Point>& path, int delta);
    std::vector<std::pair<Point, Point>> buildSegments(const std::vector<Point>& path) const;
};

#endif
