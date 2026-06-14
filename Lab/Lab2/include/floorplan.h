#pragma once
#include "Types.h"
#include "BStarTree.h"
#include "SA.h"
#include <string>
#include <chrono>
#include <unordered_map>

class Floorplanner {
public:
    void solve(const std::string& blkFile,
               const std::string& netFile,
               const std::string& outFile,
               double alpha);

private:
    // Timing
    std::chrono::steady_clock::time_point startTime_;
    double elapsed() const;

    // Data
    double alpha_    = 0.5;
    int    outlineW_ = 0, outlineH_ = 0;

    std::vector<Block>    blocks_;
    std::vector<Terminal> terminals_;
    std::vector<Net>      nets_;
    std::unordered_map<std::string,int> nameToBlock_;
    std::unordered_map<std::string,int> nameToTerminal_;

    // SA lambda (penalty weight)
    double lambda_ = 1.0;

    // ── Parse ──
    void parseBlock(const std::string& f);
    void parseNet  (const std::string& f);

    // SA pipeline
    void run(double timeLimitSec);

    // Cost
    long long calcHPWL    () const;
    long long calcTrueCost(int cw, int ch, long long hpwl) const;
    double    calcPenalized(int cw, int ch, long long hpwl) const;

    // Output
    void writeOutput(const std::string& f) const;
};