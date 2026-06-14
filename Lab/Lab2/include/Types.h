#pragma once
#include <string>
#include <vector>

// ─── Block ───────────────────────────────────────────────────────────────────
struct Block {
    std::string name;
    int w, h;           // current (possibly rotated)
    int origW, origH;
    int x, y;           // lower-left after packing
    int cx, cy;         // center (floor): x + w/2, y + h/2
};

// ─── Terminal ────────────────────────────────────────────────────────────────
struct Terminal {
    std::string name;
    int x, y;
};

// ─── Net ─────────────────────────────────────────────────────────────────────
struct Net {
    std::vector<std::string> members;
};

// ─── Chip dimensions helper ──────────────────────────────────────────────────
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