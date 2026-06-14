#pragma once
#include <string>
#include <vector>

struct Block {
    std::string name;
    int w, h;           // current (possibly rotated)
    int origW, origH;
    int x, y;           // lower-left after packing
    int cx, cy;         // center (floor): x + w/2, y + h/2
};

struct Terminal {
    std::string name;
    int x, y;
};

struct Net {
    std::vector<std::string> members;
};

inline int chipW(const std::vector<Block>& blocks) {
    int m = 0;
    for (auto& b : blocks) m = std::max(m, b.x + b.w);
    return m;
}
inline int chipH(const std::vector<Block>& blocks) {
    int m = 0;
    for (auto& b : blocks) m = std::max(m, b.y + b.h);
    return m;
}