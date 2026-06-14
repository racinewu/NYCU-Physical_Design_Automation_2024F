#pragma once
#include "Types.h"
#include <map>
#include <vector>

// ─── B*-Tree node ────────────────────────────────────────────────────────────
struct BNode {
    int    idx;
    BNode* left;
    BNode* right;
    BNode* parent;
    explicit BNode(int i)
        : idx(i), left(nullptr), right(nullptr), parent(nullptr) {}
};

// ─── Contour ─────────────────────────────────────────────────────────────────
class Contour {
public:
    void clear();
    int  query(int x1, int x2) const;
    void update(int x1, int x2, int h);
private:
    std::map<int,int> seg_;
};

// ─── BStarTree ───────────────────────────────────────────────────────────────
class BStarTree {
public:
    BStarTree() : root_(nullptr) {}
    ~BStarTree() { clear(); }

    BStarTree(const BStarTree&)            = delete;
    BStarTree& operator=(const BStarTree&) = delete;
    BStarTree(BStarTree&&) noexcept;
    BStarTree& operator=(BStarTree&&) noexcept;

    void      init(const std::vector<Block>& blocks);
    void      clear();
    BStarTree clone() const;

    void pack(std::vector<Block>& blocks) const;
    int  size() const { return (int)nodes_.size(); }

    // ── Move / Undo ──────────────────────────────────────────────────────────
    // PtrPatch: records one pointer field before it was modified.
    struct PtrPatch { BNode** slot; BNode* oldVal; };

    struct Move {
        enum Type { ROTATE, SWAP, MOVE_NODE } type = ROTATE;
        // ROTATE
        int rotIdx = -1, oldW = 0, oldH = 0;
        // SWAP
        int swapA = -1, swapB = -1;
        // MOVE_NODE — every touched pointer recorded here
        std::vector<PtrPatch> patches;
    };

    Move rotate  (int idx, std::vector<Block>& blocks);
    Move swapNode(int a, int b);
    Move moveNode(int idx, int newParIdx, bool asLeft);
    void undo    (const Move& mv, std::vector<Block>& blocks);

private:
    BNode*              root_;
    std::vector<BNode*> nodes_;

    std::vector<PtrPatch> detachNode(BNode* n);
    std::vector<PtrPatch> insertNode(BNode* n, BNode* par, bool asLeft);

    static bool isAncestor(const BNode* anc, const BNode* n);
    BNode* copySubtree(const BNode* src, BNode* par,
                       std::vector<BNode*>& nv) const;
};