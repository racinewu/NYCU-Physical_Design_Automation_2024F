#pragma once

#include <string>
#include <vector>
#include <map>

// A tile covers the half-open region [x1, x2) x [y1, y2).
// Because the right/top edges are excluded, point finding automatically
// follows the rule "right edge and top edge do not belong to the block".
struct Tile
{
    int x1, y1, x2, y2;
    int id; // 0 = space tile, > 0 = block index

    // Corner stitches:
    //   tr : topmost  right  neighbor (points right, from upper-right corner)
    //   rt : rightmost top   neighbor (points up,    from upper-right corner)
    //   bl : bottommost left neighbor (points left,  from lower-left corner)
    //   lb : leftmost bottom neighbor (points down,  from lower-left corner)
    Tile *tr, *rt, *bl, *lb;

    Tile(int X1, int Y1, int X2, int Y2, int i)
        : x1(X1), y1(Y1), x2(X2), y2(Y2), id(i),
          tr(nullptr), rt(nullptr), bl(nullptr), lb(nullptr) {}

    bool contains(int x, int y) const
    {
        return x >= x1 && x < x2 && y >= y1 && y < y2;
    }
};

class CornerStitching
{
public:
    CornerStitching();
    ~CornerStitching();

    void solve(const std::string &inputFile, const std::string &outputFile);

private:
    int W, H;             // outline size
    long long tileCount;  // number of tiles currently in the plane
    Tile *hint;           // last visited tile, start point for searches

    // Free-list recycling: merged-away tiles are marked dead and reused by newTile().
    // `pool` owns every allocation so the destructor frees everything exactly once.
    // Never iterate pool for live tiles; walk the stitch graph instead.
    std::vector<Tile *> pool;
    std::vector<Tile *> freeList;
    std::map<int, Tile *> blocks;   // block index -> block tile (sorted)

    Tile *newTile(int x1, int y1, int x2, int y2, int id);

    // Point finding
    Tile *findPoint(int x, int y);

    // Structure-maintaining primitives. Each one keeps every corner
    // stitch of the affected tiles and of their neighbors consistent.
    Tile *splitH(Tile *t, int y); // split at horizontal line y, return upper part
    Tile *splitV(Tile *t, int x); // split at vertical line x, return right part
    void mergeV(Tile *lo, Tile *hi); // merge hi (above) into lo (below), same x-span

    Tile *tryMergeUp(Tile *t);   // merge with the space tile right above if same span
    Tile *tryMergeDown(Tile *t); // merge with the space tile right below if same span

    // Tile creation
    void insertBlock(int idx, int x, int y, int w, int h);

    // Neighbor finding on all 4 sides
    void countNeighbors(const Tile *t, long long &nBlock, long long &nSpace) const;
};
