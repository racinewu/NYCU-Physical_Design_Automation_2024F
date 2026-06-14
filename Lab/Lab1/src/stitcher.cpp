#include "stitcher.h"
#include <chrono>
#include <iostream>
#include <cassert>
#include <fstream>
#include <sstream>
#include <stdexcept>
#include <utility>

using namespace std;

// id of a tile that has been merged away and is waiting for reuse
static const int DEAD = -1;

#ifdef VERBOSE
#include <cstdio>
#define LOG(...) fprintf(stderr, __VA_ARGS__)
#else
#define LOG(...) ((void)0)
#endif

CornerStitching::CornerStitching()
    : W(0), H(0), tileCount(0), hint(nullptr) {}

CornerStitching::~CornerStitching()
{
    for (Tile *t : pool)
        delete t;
}

Tile *CornerStitching::newTile(int x1, int y1, int x2, int y2, int id)
{
    Tile *t;
    if (!freeList.empty()) // reuse a recycled tile before allocating a new one
    {
        t = freeList.back();
        freeList.pop_back();
        t->x1 = x1; t->y1 = y1; t->x2 = x2; t->y2 = y2;
        t->id = id;
        t->tr = t->rt = t->bl = t->lb = nullptr;
    }
    else
    {
        t = new Tile(x1, y1, x2, y2, id);
        pool.push_back(t);
    }
    ++tileCount;
    return t;
}

// ==================================================
// Point finding (paper 5.1): step toward (x, y) one tile at a time.
// Tile convexity guarantees convergence.
// ==================================================
Tile *CornerStitching::findPoint(int x, int y)
{
    Tile *t = hint;
    while (!t->contains(x, y))
    {
        if (y < t->y1)
            t = t->lb; // move down
        else if (y >= t->y2)
            t = t->rt; // move up
        else if (x < t->x1)
            t = t->bl; // move left
        else
            t = t->tr; // move right
        assert(t && "findPoint walked off the plane: query outside outline "
                    "or a corrupt stitch");
    }
    hint = t;
    return t;
}

// ==================================================
// splitH: split tile t along horizontal line y (t->y1 < y < t->y2).
// t keeps the lower part, a new tile "up" gets the upper part.  Returns up.
// ==================================================
Tile *CornerStitching::splitH(Tile *t, int y)
{
    assert(t && t->id != DEAD && "splitH on a dead tile");
    assert(t->y1 < y && y < t->y2 && "split line must be strictly inside");

    Tile *up = newTile(t->x1, y, t->x2, t->y2, t->id);

    up->rt = t->rt;
    up->tr = t->tr;
    up->lb = t;

    // bottommost left neighbor of up = first left neighbor crossing y
    Tile *p = t->bl;
    while (p && p->y2 <= y)
        p = p->rt;
    up->bl = p;

    // redirect stitches of surrounding tiles
    // top neighbors: their lb pointed to t -> now up
    for (p = up->rt; p && p->x2 > up->x1; p = p->bl)
        if (p->lb == t)
            p->lb = up;
    // right neighbors lying entirely above y: their bl pointed to t -> up
    for (p = up->tr; p && p->y1 >= y; p = p->lb)
        if (p->bl == t)
            p->bl = up;
    // left neighbors overlapping up: their tr pointed to t -> up
    for (p = up->bl; p && p->y1 < up->y2; p = p->rt)
        if (p->tr == t)
            p->tr = up;

    // fix t itself
    // topmost right neighbor of the lower part = first right neighbor
    // whose bottom lies below y
    p = t->tr;
    while (p && p->y1 >= y)
        p = p->lb;
    t->tr = p;
    t->rt = up;
    t->y2 = y;
    return up;
}

// ==================================================
// splitV: split tile t along vertical line x (t->x1 < x < t->x2).
// t keeps the left part, a new tile "rp" gets the right part.  Returns rp.
// ==================================================
Tile *CornerStitching::splitV(Tile *t, int x)
{
    assert(t && t->id != DEAD && "splitV on a dead tile");
    assert(t->x1 < x && x < t->x2 && "split line must be strictly inside");

    Tile *rp = newTile(x, t->y1, t->x2, t->y2, t->id);

    rp->tr = t->tr;
    rp->rt = t->rt;
    rp->bl = t;

    // leftmost bottom neighbor of rp = first bottom neighbor crossing x
    Tile *p = t->lb;
    while (p && p->x2 <= x)
        p = p->tr;
    rp->lb = p;

    // redirect stitches of surrounding tiles
    // right neighbors: their bl pointed to t -> rp
    for (p = rp->tr; p && p->y2 > rp->y1; p = p->lb)
        if (p->bl == t)
            p->bl = rp;
    // top neighbors lying entirely right of x: their lb pointed to t -> rp
    for (p = rp->rt; p && p->x1 >= x; p = p->bl)
        if (p->lb == t)
            p->lb = rp;
    // bottom neighbors overlapping rp: their rt pointed to t -> rp
    for (p = rp->lb; p && p->x1 < rp->x2; p = p->tr)
        if (p->rt == t)
            p->rt = rp;

    // fix t itself
    // rightmost top neighbor of the left part = first top neighbor
    // whose left edge lies left of x
    p = t->rt;
    while (p && p->x1 >= x)
        p = p->bl;
    t->rt = p;
    t->tr = rp;
    t->x2 = x;
    return rp;
}

// ==================================================
// mergeV: merge hi (above) into lo (below); same x-span, vertically adjacent.
// lo survives and grows upward; stitches pointing at hi are redirected to lo.
// ==================================================
void CornerStitching::mergeV(Tile *lo, Tile *hi)
{
    // Re-verify geometrically what the caller derived from stitches.
    // A failure here pinpoints a stitch corrupted by an earlier split/merge.
    assert(lo && hi && lo->id != DEAD && hi->id != DEAD && "mergeV on a dead tile");
    assert(lo->id == 0 && hi->id == 0 && "mergeV merges space tiles only");
    assert(lo->x1 == hi->x1 && lo->x2 == hi->x2 && "mergeV needs identical x-span");
    assert(lo->y2 == hi->y1 && "mergeV needs hi sitting directly on top of lo");

    Tile *p;
    // top neighbors of hi: lb -> lo
    for (p = hi->rt; p && p->x2 > hi->x1; p = p->bl)
        if (p->lb == hi)
            p->lb = lo;
    // right neighbors of hi: bl -> lo
    for (p = hi->tr; p && p->y2 > hi->y1; p = p->lb)
        if (p->bl == hi)
            p->bl = lo;
    // left neighbors of hi: tr -> lo
    for (p = hi->bl; p && p->y1 < hi->y2; p = p->rt)
        if (p->tr == hi)
            p->tr = lo;
    // the only bottom neighbor of hi is lo itself (identical x-span)

    lo->y2 = hi->y2;
    lo->rt = hi->rt;
    lo->tr = hi->tr;

    if (hint == hi)
        hint = lo;

    // poison hi so any stale pointer fails fast, then recycle it
    hi->id = DEAD;
    hi->tr = hi->rt = hi->bl = hi->lb = nullptr;
    freeList.push_back(hi);
    --tileCount;
}

// merge t with the space tile directly above when both have the same x-span
Tile *CornerStitching::tryMergeUp(Tile *t)
{
    Tile *a = t->rt; // if spans match, the only top neighbor
    if (a && a->id == 0 && a->x1 == t->x1 && a->x2 == t->x2)
    {
        mergeV(t, a);
    }
    return t;
}

// merge t with the space tile directly below when both have the same x-span
Tile *CornerStitching::tryMergeDown(Tile *t)
{
    Tile *d = t->lb; // if spans match, the only bottom neighbor
    if (d && d->id == 0 && d->x1 == t->x1 && d->x2 == t->x2)
    {
        mergeV(d, t);
        return d;
    }
    return t;
}

// ==================================================
// insertBlock (paper 5.5):
//   1) horizontal cuts along the block's top and bottom edges
//   2) walk the strips top to bottom, cutting off the left / right remainders
//   3) merge the center pieces into the block tile
//   4) merge remainders with same-span space to keep the canonical form
// ==================================================
void CornerStitching::insertBlock(int idx, int x, int y, int w, int h)
{
    const int X1 = x, Y1 = y, X2 = x + w, Y2 = y + h;

    // 1) cut along the top edge of the new block
    Tile *t = findPoint(X1, Y2 - 1);
    if (t->y2 > Y2)
        splitH(t, Y2); // keep lower piece inside the area

    // 2) cut along the bottom edge of the new block
    t = findPoint(X1, Y1);
    if (t->y1 < Y1)
        splitH(t, Y1); // upper piece lies inside the area

    // 3) walk the strips from top to bottom
    Tile *cur = findPoint(X1, Y2 - 1); // topmost strip of the column
    Tile *prevC = nullptr;             // center piece of the row above

    while (true)
    {
        // the strip spans the whole block width (area is empty, strips are maximal)
        Tile *L = nullptr, *R = nullptr, *C = cur;
        if (cur->x1 < X1)
        {
            C = splitV(cur, X1); // cur keeps the left remainder
            L = cur;
        }
        if (C->x2 > X2)
            R = splitV(C, X2); // C keeps the center [X1, X2)

        const bool lastRow = (C->y1 == Y1);

        // locate the next strip below before this row is merged around
        Tile *next = nullptr;
        if (!lastRow)
        {
            Tile *p = C->lb;
            while (p->x2 <= X1)
                p = p->tr;
            next = p;
        }

        // chain the center pieces (always identical span [X1, X2))
        if (prevC)
            mergeV(C, prevC);
        prevC = C;

        // merge remainders with same-span space above (and below on the last row)
        if (L)
        {
            L = tryMergeUp(L);
            if (lastRow)
                L = tryMergeDown(L);
        }
        if (R)
        {
            R = tryMergeUp(R);
            if (lastRow)
                R = tryMergeDown(R);
        }

        if (lastRow)
            break;
        cur = next;
    }

    // the assembled center tile is exactly [X1,X2) x [Y1,Y2): make it solid
    prevC->id = idx;
    blocks[idx] = prevC;
    hint = prevC;
}

// ==================================================
// Neighbor finding (paper 5.2): scan all four sides of t.
// Every visited tile overlaps t, so corner-only contacts are never counted.
// ==================================================
void CornerStitching::countNeighbors(const Tile *t,
                                     long long &nBlock, long long &nSpace) const
{
    const Tile *p;
    // right side: start at topmost right neighbor, trace lb downward
    for (p = t->tr; p && p->y2 > t->y1; p = p->lb)
        (p->id > 0 ? nBlock : nSpace)++;
    // left side: start at bottommost left neighbor, trace rt upward
    for (p = t->bl; p && p->y1 < t->y2; p = p->rt)
        (p->id > 0 ? nBlock : nSpace)++;
    // top side: start at rightmost top neighbor, trace bl leftward
    for (p = t->rt; p && p->x2 > t->x1; p = p->bl)
        (p->id > 0 ? nBlock : nSpace)++;
    // bottom side: start at leftmost bottom neighbor, trace tr rightward
    for (p = t->lb; p && p->x1 < t->x2; p = p->tr)
        (p->id > 0 ? nBlock : nSpace)++;
}

// Entry point
void CornerStitching::solve(const string &inputFile,
                            const string &outputFile)
{
    ifstream fin(inputFile);
    if (!fin)
        throw runtime_error("cannot open input file: " + inputFile);
    ofstream fout(outputFile);
    if (!fout)
        throw runtime_error("cannot open output file: " + outputFile);

    struct Command
    {
        bool isPoint;
        int idx, x, y, w, h; // point queries use x / y only
    };

    fin >> W >> H;

    vector<Command> cmds;
    size_t nBlockCmd = 0, nPointCmd = 0;
    string tok;
    while (fin >> tok)
    {
        Command c;
        if (tok == "P" || tok == "p")
        {
            c.isPoint = true;
            c.idx = c.w = c.h = 0;
            fin >> c.x >> c.y;
            ++nPointCmd;
        }
        else
        {
            c.isPoint = false;
            c.idx = stoi(tok);
            fin >> c.x >> c.y >> c.w >> c.h;
            ++nBlockCmd;
        }
        cmds.push_back(c);
    }

    cout << "[Corner Stitching] outline: " << W << " x " << H << ", "
              << nBlockCmd << " block insertion + "
              << nPointCmd << " point finding\n";

    const auto tStart = chrono::steady_clock::now();

    // initial state: one space tile covering the whole outline
    hint = newTile(0, 0, W, H, 0);

    vector<pair<int, int>> points; // P-query answers, input order
    for (const Command &c : cmds)
    {
        if (c.isPoint)
        {
            Tile *res = findPoint(c.x, c.y);
            points.emplace_back(res->x1, res->y1); // at input time
            if (res->id > 0)
                LOG("[Point] (%d, %d) -> (%d, %d) block #%d\n",
                    c.x, c.y, res->x1, res->y1, res->id);
            else
                LOG("[Point] (%d, %d) -> (%d, %d) space\n",
                    c.x, c.y, res->x1, res->y1);
        }
        else
        {
            insertBlock(c.idx, c.x, c.y, c.w, c.h);
            LOG("[Block] #%d (%d, %d) %dx%d -> tiles %lld\n",
                c.idx, c.x, c.y, c.w, c.h, tileCount);
        }
    }

    fout << tileCount << "\n";
    for (const auto &kv : blocks) // map -> ascending block index
    {
        long long nBlock = 0, nSpace = 0;
        countNeighbors(kv.second, nBlock, nSpace);
        fout << kv.first << " " << nBlock << " " << nSpace << "\n";
    }
    for (const auto &pt : points)
        fout << pt.first << " " << pt.second << "\n";

    const chrono::duration<double> elapsed = chrono::steady_clock::now() - tStart;
    cout << "[Final] tiles = " << blocks.size() << " block + "
              << (tileCount - (long long)blocks.size()) << " space, alloc "
              << pool.size() << " / free " << freeList.size()
              << ", elapsed: " << elapsed.count() << " s\n";
}