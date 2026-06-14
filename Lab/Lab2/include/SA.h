#pragma once
#include "Types.h"
#include "BStarTree.h"
#include <functional>
#include <chrono>
#include <random>
#include <climits>

class SAEngine {
public:
    struct Config {
        double coolRate        = 0.995;
        int    innerFactor     = 12;      // inner = n * innerFactor
        double totalTimeSec    = 280.0;   // wall-clock budget for all restarts
        double noImproveTimeSec= 30.0;    // restart if no improvement for this long
        int    estimateSamples = 200;
    };

    struct Result {
        long long bestTrueCost = LLONG_MAX;
        std::vector<int> x, y, w, h;
    };

    Result run(
        BStarTree&                                               tree,
        std::vector<Block>&                                      blocks,
        std::function<double(const std::vector<Block>&)>         costFn,
        std::function<long long(const std::vector<Block>&)>      trueCostFn,
        std::function<bool(const std::vector<Block>&)>           fitFn,
        std::function<void(bool /*fit*/, bool /*restart*/)>      onRestart,
        const Config& cfg);

private:
    std::mt19937 rng_;
    std::chrono::steady_clock::time_point start_;

    double elapsed() const;

    // Run one SA pass from the current tree state.
    // Returns when time is up OR no improvement for noImproveTimeSec.
    // Updates `best` if a better solution is found.
    // Returns true if the total time limit has been reached.
    bool runOnce(
        BStarTree&                                                tree,
        std::vector<Block>&                                       blocks,
        const std::function<double(const std::vector<Block>&)>&   costFn,
        const std::function<long long(const std::vector<Block>&)>&trueCostFn,
        const std::function<bool(const std::vector<Block>&)>&     fitFn,
        const std::function<void(bool,bool)>&                     onRestart,
        Result&                                                   best,
        const Config&                                             cfg);

    double estimateT0(BStarTree& tree, std::vector<Block>& blocks,
                      const std::function<double(const std::vector<Block>&)>& costFn,
                      double curCost, int samples);
    void saveBest(Result& best, const std::vector<Block>& blocks, long long tc);
};