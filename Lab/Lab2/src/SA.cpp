#include "SA.h"
#include <cmath>
#include <cstdio>
#include <algorithm>

// ── Verbose log (stderr, enabled by make VERBOSE=1) ─────────────────────────
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

void SAEngine::saveBest(Result& best, const vector<Block>& blocks, long long tc) {
    best.bestTrueCost = tc;
    int n = (int)blocks.size();
    best.x.resize(n); best.y.resize(n);
    best.w.resize(n); best.h.resize(n);
    for (int i = 0; i < n; ++i) {
        best.x[i] = blocks[i].x; best.y[i] = blocks[i].y;
        best.w[i] = blocks[i].w; best.h[i] = blocks[i].h;
    }
}

// ── Estimate T0: run `samples` random moves, average |ΔCost| ─────────────────

double SAEngine::estimateT0(BStarTree& tree, vector<Block>& blocks,
                             const function<double(const vector<Block>&)>& costFn,
                             double curCost, int samples) {
    int n = tree.size();
    uniform_int_distribution<int> ri(0, n-1);
    uniform_int_distribution<int> rm(0, 2);
    uniform_int_distribution<int> rb(0, 1);

    double sumDelta = 0;
    int    count    = 0;

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

        // Undo topology only — coordinates are recomputed by the next pack
        tree.undo(move, blocks);
    }
    tree.pack(blocks);   // leave blocks consistent with the tree

    if (count == 0 || sumDelta == 0) return 1e8;
    return (sumDelta / count) / log(2.0);   // T s.t. P(accept avg delta) = 0.5
}

// ════════════════════════════════════════════════════════════════════
//  run
// ════════════════════════════════════════════════════════════════════

SAEngine::Result SAEngine::run(
    BStarTree&                                           tree,
    vector<Block>&                                       blocks,
    function<double(const vector<Block>&)>               costFn,
    function<long long(const vector<Block>&)>            trueCostFn,
    function<bool(const vector<Block>&)>                 fitFn,
    function<void(bool)>                                 onLambdaUpdate,
    const Config& cfg)
{
    start_ = steady_clock::now();
    int n   = tree.size();
    int inner = max(80, n * cfg.innerFactor);

    uniform_int_distribution<int> rm(0, 2);
    uniform_int_distribution<int> ri(0, n-1);
    uniform_int_distribution<int> rb(0, 1);
    uniform_real_distribution<double> rd(0.0, 1.0);

    // ── Initial state ──
    tree.pack(blocks);
    double curCost = costFn(blocks);
    bool   fit     = fitFn(blocks);

    Result    best;
    BStarTree bestTree;          // tree snapshot of the best solution
    bool      hasBest = false;

    auto trySaveBest = [&]() {
        if (!fit) return;
        long long tc = trueCostFn(blocks);
        if (tc < best.bestTrueCost) {
            saveBest(best, blocks, tc);
            bestTree = tree.clone();
            hasBest  = true;
        }
    };
    trySaveBest();

    // ── T0 estimation ──
    double T = estimateT0(tree, blocks, costFn, curCost, cfg.estimateSamples);
    curCost  = costFn(blocks);
    fit      = fitFn(blocks);
    trySaveBest();

    LOG("[%5.1fs] SA start: n=%d inner=%d T0=%.4g cost=%.0f %s\n",
        elapsed(), n, inner, T, curCost, fit ? "fit" : "out");

    double T0      = T;
    double Tmin    = T * 1e-5;   // reheat threshold
    double reheatT = T0 * 0.1;   // halves on every reheat (finer over time)

    int round = 0;

    // ── Main SA loop — restart from best with decaying reheat ──
    while (elapsed() < cfg.timeLimitSec) {

        for (int iter = 0; iter < inner; ++iter) {
            int mv = rm(rng_);
            int ia = ri(rng_), ib = ri(rng_);
            if (ia == ib) ib = (ib + 1) % n;

            // Apply perturbation
            BStarTree::Move move;
            if      (mv == 0) move = tree.rotate  (ia, blocks);
            else if (mv == 1) move = tree.swapNode (ia, ib);
            else              move = tree.moveNode (ia, ib, (bool)rb(rng_));

            tree.pack(blocks);
            double newCost = costFn(blocks);
            double delta   = newCost - curCost;

            bool accept = (delta < 0.0) || (rd(rng_) < exp(-delta / T));

            if (accept) {
                curCost = newCost;
                fit     = fitFn(blocks);
                trySaveBest();
            } else {
                // Undo topology only — no repack needed: pack() recomputes
                // all coordinates from scratch on the next iteration.
                tree.undo(move, blocks);
            }
        }

        T *= cfg.coolRate;
        onLambdaUpdate(fit);

        // lambda may have changed; refresh curCost under the new landscape
        tree.pack(blocks);
        curCost = costFn(blocks);
        fit     = fitFn(blocks);

        // Reheat: restart from the BEST solution with a smaller temperature
        if (T < Tmin) {
            T       = max(reheatT, Tmin * 10.0);
            reheatT = max(reheatT * 0.5, Tmin * 10.0);
            if (hasBest) {
                tree = bestTree.clone();
                for (int i = 0; i < n; ++i) {
                    blocks[i].w = best.w[i];
                    blocks[i].h = best.h[i];
                }
                tree.pack(blocks);
                curCost = costFn(blocks);
                fit     = fitFn(blocks);
            }
            LOG("[%5.1fs] reheat: T=%.4g best=%lld\n",
                elapsed(), T, best.bestTrueCost);
        }

        if (++round % 200 == 0)
            LOG("[%5.1fs] round %d: T=%.4g cost=%.0f best=%lld %s\n",
                elapsed(), round, T, curCost, best.bestTrueCost,
                fit ? "fit" : "out");
    }

    LOG("[%5.1fs] SA done: rounds=%d best=%lld\n",
        elapsed(), round, best.bestTrueCost);

    return best;
}