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
        int    innerFactor     = 12;     // inner loop = n * innerFactor
        double timeLimitSec    = 270.0;
        int    estimateSamples = 200;
    };

    struct Result {
        long long bestTrueCost = LLONG_MAX;
        std::vector<int> x, y, w, h;
    };

    // costFn    : penalized cost used for SA acceptance
    // trueCostFn: integer alpha*A+(1-alpha)*W, only used to track best
    // fitFn     : true when placement is inside outline
    // onLambdaUpdate: called each outer iteration so Floorplanner can
    //                 adjust lambda before next inner loop
    Result run(
        BStarTree&                                               tree,
        std::vector<Block>&                                      blocks,
        std::function<double(const std::vector<Block>&)>         costFn,
        std::function<long long(const std::vector<Block>&)>      trueCostFn,
        std::function<bool(const std::vector<Block>&)>           fitFn,
        std::function<void(bool /*fit*/)>                        onLambdaUpdate,
        const Config& cfg);

private:
    std::mt19937 rng_{42};
    std::chrono::steady_clock::time_point start_;

    double elapsed() const;
    double estimateT0(BStarTree& tree, std::vector<Block>& blocks,
                      const std::function<double(const std::vector<Block>&)>& costFn,
                      double curCost, int samples);
    void saveBest(Result& best, const std::vector<Block>& blocks, long long tc);
};