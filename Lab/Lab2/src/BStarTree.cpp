#include "BStarTree.h"
#include <numeric>
#include <algorithm>
#include <stack>

using namespace std;

// ════════════════════════════════════════════════════════════════════
//  Contour
// ════════════════════════════════════════════════════════════════════

void Contour::clear() { seg_.clear(); seg_[0] = 0; }

int Contour::query(int x1, int x2) const {
    auto it = seg_.upper_bound(x1); --it;
    int h = 0;
    while (it != seg_.end() && it->first < x2) { h = max(h, it->second); ++it; }
    return h;
}

void Contour::update(int x1, int x2, int h) {
    auto itR = seg_.upper_bound(x2); --itR; int hR = itR->second;
    seg_.erase(seg_.upper_bound(x1), seg_.upper_bound(x2));
    seg_[x1] = h;
    if (!seg_.count(x2)) seg_[x2] = hR;
}

// ════════════════════════════════════════════════════════════════════
//  BStarTree — lifecycle
// ════════════════════════════════════════════════════════════════════

BStarTree::BStarTree(BStarTree&& o) noexcept
    : root_(o.root_), nodes_(move(o.nodes_)) { o.root_ = nullptr; }

BStarTree& BStarTree::operator=(BStarTree&& o) noexcept {
    if (this != &o) { clear(); root_ = o.root_; nodes_ = move(o.nodes_); o.root_ = nullptr; }
    return *this;
}

void BStarTree::clear() {
    if (!root_) return;
    stack<BNode*> s; s.push(root_);
    while (!s.empty()) {
        BNode* n = s.top(); s.pop();
        if (n->right) s.push(n->right);
        if (n->left)  s.push(n->left);
        delete n;
    }
    root_ = nullptr; nodes_.clear();
}

// ════════════════════════════════════════════════════════════════════
//  init
// ════════════════════════════════════════════════════════════════════

void BStarTree::init(const vector<Block>& blocks) {
    clear();
    int n = (int)blocks.size();
    nodes_.resize(n);
    for (int i = 0; i < n; ++i) nodes_[i] = new BNode(i);

    vector<int> ord(n); iota(ord.begin(), ord.end(), 0);
    sort(ord.begin(), ord.end(), [&](int a, int b) {
        return (long long)blocks[a].w * blocks[a].h >
               (long long)blocks[b].w * blocks[b].h;
    });
    root_ = nodes_[ord[0]];
    BNode* cur = root_;
    for (int i = 1; i < n; ++i) {
        cur->left = nodes_[ord[i]];
        nodes_[ord[i]]->parent = cur;
        cur = cur->left;
    }
}

// ════════════════════════════════════════════════════════════════════
//  clone
// ════════════════════════════════════════════════════════════════════

BNode* BStarTree::copySubtree(const BNode* src, BNode* par,
                               vector<BNode*>& nv) const {
    if (!src) return nullptr;
    BNode* n = new BNode(src->idx);
    n->parent = par; nv[src->idx] = n;
    n->left  = copySubtree(src->left,  n, nv);
    n->right = copySubtree(src->right, n, nv);
    return n;
}

BStarTree BStarTree::clone() const {
    BStarTree t;
    t.nodes_.resize(nodes_.size());
    t.root_ = copySubtree(root_, nullptr, t.nodes_);
    return t;
}

// ════════════════════════════════════════════════════════════════════
//  pack — iterative DFS
// ════════════════════════════════════════════════════════════════════

void BStarTree::pack(vector<Block>& blocks) const {
    if (!root_) return;
    Contour contour; contour.clear();
    struct Frame { BNode* n; bool visited; };
    stack<Frame> s; s.push({root_, false});
    while (!s.empty()) {
        Frame& f = s.top();
        if (!f.n) { s.pop(); continue; }
        if (!f.visited) {
            f.visited = true;
            Block& b = blocks[f.n->idx];
            if (!f.n->parent) b.x = 0;
            else { Block& p = blocks[f.n->parent->idx]; b.x = (f.n == f.n->parent->left) ? p.x + p.w : p.x; }
            b.y = contour.query(b.x, b.x + b.w);
            contour.update(b.x, b.x + b.w, b.y + b.h);
            b.cx = b.x + b.w / 2;
            b.cy = b.y + b.h / 2;
            s.push({f.n->right, false}); s.push({f.n->left, false});
        } else s.pop();
    }
}

// ════════════════════════════════════════════════════════════════════
//  Patch helper
// ════════════════════════════════════════════════════════════════════

static BStarTree::PtrPatch makePatch(BNode** slot, BNode* newVal) {
    BStarTree::PtrPatch p { slot, *slot };
    *slot = newVal;
    return p;
}

// ════════════════════════════════════════════════════════════════════
//  detachNode
// ════════════════════════════════════════════════════════════════════

vector<BStarTree::PtrPatch> BStarTree::detachNode(BNode* n) {
    vector<PtrPatch> patches;
    BNode* par = n->parent;
    BNode* lc  = n->left;
    BNode* rc  = n->right;

    // Build replacement subtree: chain lc (with rc as its rightmost-right)
    BNode* repl = nullptr;
    if (lc) {
        repl = lc;
        if (rc) {
            BNode* cur = lc;
            while (cur->right) cur = cur->right;
            patches.push_back(makePatch(&cur->right, rc));
            patches.push_back(makePatch(&rc->parent,  cur));
        }
        patches.push_back(makePatch(&lc->parent, par));
    } else if (rc) {
        repl = rc;
        patches.push_back(makePatch(&rc->parent, par));
    }

    // Update parent (or root_)
    if (!par) {
        patches.push_back(makePatch(&root_, repl));
    } else {
        if (par->left == n) patches.push_back(makePatch(&par->left,  repl));
        else                patches.push_back(makePatch(&par->right, repl));
    }

    // Clear n's own fields
    patches.push_back(makePatch(&n->parent, nullptr));
    patches.push_back(makePatch(&n->left,   nullptr));
    patches.push_back(makePatch(&n->right,  nullptr));
    return patches;
}

// ════════════════════════════════════════════════════════════════════
//  insertNode
// ════════════════════════════════════════════════════════════════════

vector<BStarTree::PtrPatch> BStarTree::insertNode(BNode* n, BNode* par,
                                                   bool asLeft) {
    vector<PtrPatch> patches;
    patches.push_back(makePatch(&n->parent, par));
    if (asLeft) {
        BNode* old = par->left;
        patches.push_back(makePatch(&n->left,   old));
        patches.push_back(makePatch(&par->left,  n));
        if (old) patches.push_back(makePatch(&old->parent, n));
    } else {
        BNode* old = par->right;
        patches.push_back(makePatch(&n->right,  old));
        patches.push_back(makePatch(&par->right, n));
        if (old) patches.push_back(makePatch(&old->parent, n));
    }
    return patches;
}

// ════════════════════════════════════════════════════════════════════
//  isAncestor
// ════════════════════════════════════════════════════════════════════

bool BStarTree::isAncestor(const BNode* anc, const BNode* n) {
    while (n) { if (n == anc) return true; n = n->parent; }
    return false;
}

// ════════════════════════════════════════════════════════════════════
//  Perturbations
// ════════════════════════════════════════════════════════════════════

BStarTree::Move BStarTree::rotate(int idx, vector<Block>& blocks) {
    Move mv; mv.type = Move::ROTATE; mv.rotIdx = idx;
    mv.oldW = blocks[idx].w; mv.oldH = blocks[idx].h;
    swap(blocks[idx].w, blocks[idx].h);
    return mv;
}

BStarTree::Move BStarTree::swapNode(int a, int b) {
    Move mv; mv.type = Move::SWAP; mv.swapA = a; mv.swapB = b;
    swap(nodes_[a]->idx, nodes_[b]->idx);
    swap(nodes_[a], nodes_[b]);
    return mv;
}

BStarTree::Move BStarTree::moveNode(int idx, int newParIdx, bool asLeft) {
    Move mv; mv.type = Move::MOVE_NODE;
    if (idx == newParIdx) return mv;
    BNode* n    = nodes_[idx];
    BNode* newP = nodes_[newParIdx];
    if (isAncestor(n, newP)) return mv;
    auto dp = detachNode(n);
    auto ip = insertNode(n, newP, asLeft);
    mv.patches.insert(mv.patches.end(), dp.begin(), dp.end());
    mv.patches.insert(mv.patches.end(), ip.begin(), ip.end());
    return mv;
}

// ════════════════════════════════════════════════════════════════════
//  undo
// ════════════════════════════════════════════════════════════════════

void BStarTree::undo(const Move& mv, vector<Block>& blocks) {
    switch (mv.type) {
    case Move::ROTATE:
        blocks[mv.rotIdx].w = mv.oldW;
        blocks[mv.rotIdx].h = mv.oldH;
        break;
    case Move::SWAP:
        swap(nodes_[mv.swapA]->idx, nodes_[mv.swapB]->idx);
        swap(nodes_[mv.swapA], nodes_[mv.swapB]);
        break;
    case Move::MOVE_NODE:
        for (int i = (int)mv.patches.size()-1; i >= 0; --i)
            *(mv.patches[i].slot) = mv.patches[i].oldVal;
        break;
    }
}