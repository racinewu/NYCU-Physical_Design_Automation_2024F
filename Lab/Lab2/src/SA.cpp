#include "SA.h"
#include <cmath>
#include <cstdio>
#include <algorithm>
#include <numeric>

#ifdef VERBOSE
#  define LOG(...) fprintf(stderr, __VA_ARGS__)
#else
#  define LOG(...) ((void)0)
#endif

using namespace std;
using namespace std::chrono;

double SAEngine::elapsed() const {
    return duration_cast<duration<double>>(steady_clock::now() - start_).count();
}

// saveBest: snapshot block coordinates and true cost into result.
void SAEngine::saveBest(Result& best, const vector<Block>& blocks, long long tc) {
    best.bestTrueCost = tc;
    int n = (int)blocks.size();
    best.x.resize(n); best.y.resize(n); best.w.resize(n); best.h.resize(n);
    for (int i = 0; i < n; ++i) {
        best.x[i] = blocks[i].x; best.y[i] = blocks[i].y;
        best.w[i] = blocks[i].w; best.h[i] = blocks[i].h;
    }
}

// estimateT0: sample |ΔCost| over random moves; T0 = avg / ln2 gives ~50% acceptance.
double SAEngine::estimateT0(BStarTree& tree, vector<Block>& blocks,
                             const function<double(const vector<Block>&)>& costFn,
                             double curCost, int samples) {
    int n = tree.size();
    uniform_int_distribution<int> ri(0, n-1), rm(0, 2), rb(0, 1);
    double sumDelta = 0; int count = 0;
    for (int t = 0; t < samples; ++t) {
        int mv = rm(rng_), ia = ri(rng_), ib = ri(rng_);
        if (ia == ib) ib = (ib + 1) % n;
        BStarTree::Move move;
        if      (mv == 0) move = tree.rotate  (ia, blocks);
        else if (mv == 1) move = tree.swapNode (ia, ib);
        else              move = tree.moveNode (ia, ib, (bool)rb(rng_));
        tree.pack(blocks);
        sumDelta += fabs(costFn(blocks) - curCost);
        ++count;
        tree.undo(move, blocks);
    }
    tree.pack(blocks);
    if (count == 0 || sumDelta == 0) return 1e8;
    return (sumDelta / count) / log(2.0);
}

// ==================================================
// runOnce: one SA pass with geometric cooling and reheat.
// Returns true = total time up; false = no-improve timeout (triggers restart).
// On reheat: restore local best, halve reheat temperature to shift toward exploitation.
// ==================================================
bool SAEngine::runOnce(
    BStarTree& tree, vector<Block>& blocks,
    const function<double(const vector<Block>&)>&             costFn,
    const function<long long(const vector<Block>&)>&          trueCostFn,
    const function<bool(const vector<Block>&)>&               fitFn,
    const function<void(bool,bool)>&                          onRestart,
    Result& best, const Config& cfg)
{
    int n     = tree.size();
    int inner = max(80, n * cfg.innerFactor);
    uniform_int_distribution<int> rm(0, 2), ri(0, n-1), rb(0, 1);
    uniform_real_distribution<double> rd(0.0, 1.0);

    tree.pack(blocks);
    double curCost = costFn(blocks);
    bool   fit     = fitFn(blocks);

    // local best snapshot used to restore state on reheat
    BStarTree localBestTree;
    bool      hasLocalBest = false;
    auto tryLocalSave = [&]() {
        if (!fit) return;
        long long tc = trueCostFn(blocks);
        if (tc < best.bestTrueCost) {
            saveBest(best, blocks, tc);
            localBestTree = tree.clone();
            hasLocalBest  = true;
        }
    };
    tryLocalSave();

    double T = estimateT0(tree, blocks, costFn, curCost, cfg.estimateSamples);
    curCost   = costFn(blocks); fit = fitFn(blocks); tryLocalSave();

    LOG("[%5.1fs] SA pass start: T0=%.4g cost=%.0f %s\n", elapsed(), T, curCost, fit ? "fit" : "out");

    double T0              = T;
    double Tmin            = T * 1e-5;
    double reheatT         = T0 * 0.1;
    double lastImproveTime = elapsed();
    int    round           = 0;

    while (true) {
        double now = elapsed();
        if (now >= cfg.totalTimeSec)                       return true;
        if (now - lastImproveTime >= cfg.noImproveTimeSec) return false;

        for (int iter = 0; iter < inner; ++iter) {
            int mv = rm(rng_), ia = ri(rng_), ib = ri(rng_);
            if (ia == ib) ib = (ib + 1) % n;

            BStarTree::Move move;
            if      (mv == 0) move = tree.rotate  (ia, blocks);
            else if (mv == 1) move = tree.swapNode (ia, ib);
            else              move = tree.moveNode (ia, ib, (bool)rb(rng_));

            tree.pack(blocks);
            double newCost = costFn(blocks);
            bool accept    = (newCost < curCost) || (rd(rng_) < exp(-(newCost - curCost) / T));

            if (accept) {
                curCost = newCost; fit = fitFn(blocks);
                long long prev = best.bestTrueCost;
                tryLocalSave();
                if (best.bestTrueCost < prev) lastImproveTime = elapsed();
            } else {
                tree.undo(move, blocks);
            }
        }

        T *= cfg.coolRate;
        onRestart(fit, false);   // update lambda each round

        // refresh curCost after lambda change
        tree.pack(blocks); curCost = costFn(blocks); fit = fitFn(blocks);

        if (T < Tmin) {
            T       = max(reheatT, Tmin * 10.0);
            reheatT = max(reheatT * 0.5, Tmin * 10.0);
            if (hasLocalBest) {
                tree = localBestTree.clone();
                for (int i = 0; i < n; ++i) { blocks[i].w = best.w[i]; blocks[i].h = best.h[i]; }
                tree.pack(blocks); curCost = costFn(blocks); fit = fitFn(blocks);
            }
            LOG("[%5.1fs] reheat: T=%.4g best=%lld\n", elapsed(), T, best.bestTrueCost);
        }

        if (++round % 200 == 0)
            LOG("[%5.1fs] round %d: T=%.4g cost=%.0f best=%lld %s\n", elapsed(), round, T, curCost, best.bestTrueCost, fit ? "fit" : "out");
    }
}

// ==================================================
// run: outer restart loop; seeds rng from wall-clock so every execution differs.
// Each restart re-seeds and shuffles the initial tree order.
// ==================================================
SAEngine::Result SAEngine::run(
    BStarTree& tree, vector<Block>& blocks,
    function<double(const vector<Block>&)>               costFn,
    function<long long(const vector<Block>&)>            trueCostFn,
    function<bool(const vector<Block>&)>                 fitFn,
    function<void(bool, bool)>                           onRestart,
    const Config& cfg)
{
    start_ = steady_clock::now();
    rng_.seed((uint32_t)duration_cast<nanoseconds>(start_.time_since_epoch()).count());

    Result best;
    int    restartCount = 0;

    while (elapsed() < cfg.totalTimeSec) {
        if (restartCount > 0) {
            // fresh seed to avoid correlated restarts
            rng_.seed((uint32_t)duration_cast<nanoseconds>(
                          steady_clock::now().time_since_epoch()).count()
                      + restartCount * 1000003u);

            int n = (int)blocks.size();
            vector<int> ord(n); iota(ord.begin(), ord.end(), 0);
            shuffle(ord.begin(), ord.end(), rng_);

            for (int i = 0; i < n; ++i) { blocks[i].w = blocks[i].origW; blocks[i].h = blocks[i].origH; }
            tree.init(blocks, ord);

            onRestart(false, true);   // signal restart so lambda resets
            LOG("[%5.1fs] restart #%d\n", elapsed(), restartCount);
        }

        bool done = runOnce(tree, blocks, costFn, trueCostFn, fitFn, onRestart, best, cfg);

        LOG("[%5.1fs] pass %d ended: best=%lld %s\n",
            elapsed(), restartCount, best.bestTrueCost,
            done ? "(time limit)" : "(no improve)");

        ++restartCount;
        if (done) break;
    }

    LOG("[%5.1fs] SA total: %d passes, best=%lld\n", elapsed(), restartCount, best.bestTrueCost);

    return best;
}