#ifndef TYPES_H
#define TYPES_H

#include <string>
#include <vector>

struct Pin {
    int x = 0;
    int y = 0;
};

struct Point {
    int x = 0;
    int y = 0;
};

struct Net {
    int id = 0;
    std::string name;
    int numPins = 0;
    std::vector<Pin> pins;
    std::vector<Point> routePath;
};

#endif
