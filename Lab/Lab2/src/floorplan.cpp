#include "floorplan.h"
#include <fstream>
#include <iostream>
#include <stdexcept>
#include <climits>
#include <algorithm>

using namespace std;
using namespace std::chrono;

double Floorplanner::elapsed() const {
    return duration_cast<duration<double>>(steady_clock::now() - startTime_).count();
}

// parseBlock: read outline size, block dimensions, and terminal positions.
void Floorplanner::parseBlock(const string& f) {
    ifstream in(f);
    if (!in) throw runtime_error("cannot open block file: " + f);
    string tok;
    in >> tok >> outlineW_ >> outlineH_;
    int nb, nt; in >> tok >> nb >> tok >> nt;
    blocks_.resize(nb);
    for (int i = 0; i < nb; ++i) {
        in >> blocks_[i].name >> blocks_[i].w >> blocks_[i].h;
        blocks_[i].origW = blocks_[i].w; blocks_[i].origH = blocks_[i].h;
        nameToBlock_[blocks_[i].name] = i;
    }
    terminals_.resize(nt);
    for (int i = 0; i < nt; ++i) {
        string dummy;
        in >> terminals_[i].name >> dummy >> terminals_[i].x >> terminals_[i].y;
        nameToTerminal_[terminals_[i].name] = i;
    }
}

// parseNet: read net membership lists.
void Floorplanner::parseNet(const string& f) {
    ifstream in(f);
    if (!in) throw runtime_error("cannot open net file: " + f);
    string tok; int nn;
    in >> tok >> nn; nets_.resize(nn);
    for (int i = 0; i < nn; ++i) {
        int d; in >> tok >> d;
        nets_[i].members.resize(d);
        for (int j = 0; j < d; ++j) in >> nets_[i].members[j];
    }
}

// calcHPWL: sum HPWL over all nets using pre-computed cx/cy from pack().
long long Floorplanner::calcHPWL() const {
    long long tot = 0;
    for (auto& net : nets_) {
        int xmn = INT_MAX, xmx = INT_MIN, ymn = INT_MAX, ymx = INT_MIN;
        for (auto& nm : net.members) {
            int cx, cy;
            auto it = nameToBlock_.find(nm);
            if (it != nameToBlock_.end()) {
                cx = blocks_[it->second].cx; cy = blocks_[it->second].cy;
            } else {
                auto& t = terminals_[nameToTerminal_.at(nm)]; cx = t.x; cy = t.y;
            }
            xmn = min(xmn, cx); xmx = max(xmx, cx);
            ymn = min(ymn, cy); ymx = max(ymx, cy);
        }
        tot += (xmx - xmn) + (ymx - ymn);
    }
    return tot;
}

// cost = int(alpha*A + (1-alpha)*W), consistent with verifier.
long long Floorplanner::calcTrueCost(int cw, int ch, long long hpwl) const {
    return (long long)(alpha_ * (double)cw * (double)ch + (1.0 - alpha_) * (double)hpwl);
}

// penalized cost adds lambda * outline violation for SA acceptance.
double Floorplanner::calcPenalized(int cw, int ch, long long hpwl) const {
    double cost = alpha_ * (double)cw * ch + (1.0 - alpha_) * (double)hpwl;
    if (cw > outlineW_) cost += lambda_ * (double)(cw - outlineW_) * outlineH_;
    if (ch > outlineH_) cost += lambda_ * (double)(ch - outlineH_) * outlineW_;
    return cost;
}

// writeOutput: six-part report per spec (cost, hpwl, area, wh, time, coords).
void Floorplanner::writeOutput(const string& f) const {
    ofstream out(f);
    if (!out) throw runtime_error("cannot open output file: " + f);
    int       cw   = chipW(blocks_), ch = chipH(blocks_);
    long long hpwl = calcHPWL();
    long long cost = calcTrueCost(cw, ch, hpwl);
    out << cost << "\n" << hpwl << "\n" << (long long)cw*ch << "\n"
        << cw << " " << ch << "\n" << elapsed() << "\n";
    for (auto& b : blocks_)
        out << b.name << " " << b.x << " " << b.y << " "
            << (b.x+b.w) << " " << (b.y+b.h) << "\n";
}

// ==================================================
// run: build B*-tree and cost/fit closures, then hand off to SAEngine.
// lambda starts proportional to outline/block area ratio.
// onRestart resets lambda on each new pass so exploration starts fresh.
// ==================================================
void Floorplanner::run(double timeLimitSec) {
    BStarTree tree;
    tree.init(blocks_);

    // closures capture *this; blocks_ is mutated in-place by SAEngine
    auto costFn = [&](const vector<Block>& blks) -> double {
        return calcPenalized(chipW(blks), chipH(blks), calcHPWL());
    };
    auto trueCostFn = [&](const vector<Block>& blks) -> long long {
        return calcTrueCost(chipW(blks), chipH(blks), calcHPWL());
    };
    auto fitFn = [&](const vector<Block>& blks) -> bool {
        return chipW(blks) <= outlineW_ && chipH(blks) <= outlineH_;
    };

    double initLambda = lambda_;
    auto onRestart = [&](bool fit, bool restart) {
        if (restart) lambda_ = initLambda;
        else if (!fit) lambda_ = min(lambda_ * 1.08, 1e12);
        else           lambda_ = max(lambda_ * 0.99,  1.0);
    };

    SAEngine::Config cfg;
    cfg.totalTimeSec     = timeLimitSec;
    cfg.noImproveTimeSec = max(20.0, timeLimitSec * 0.12);

    SAEngine sa;
    auto result = sa.run(tree, blocks_, costFn, trueCostFn, fitFn, onRestart, cfg);

    // apply best placement found across all passes
    if (result.bestTrueCost != LLONG_MAX) {
        int n = (int)blocks_.size();
        for (int i = 0; i < n; ++i) {
            blocks_[i].x = result.x[i]; blocks_[i].y = result.y[i];
            blocks_[i].w = result.w[i]; blocks_[i].h = result.h[i];
            blocks_[i].cx = blocks_[i].x + blocks_[i].w / 2;
            blocks_[i].cy = blocks_[i].y + blocks_[i].h / 2;
        }
    }
}

// solve: parse → init lambda → run SA → write output.
void Floorplanner::solve(const string& blkFile, const string& netFile,
                         const string& outFile, double alpha) {
    startTime_ = steady_clock::now();
    alpha_     = alpha;

    parseBlock(blkFile);
    parseNet  (netFile);

    cout << "[Floorplan] outline: " << outlineW_ << " x " << outlineH_
         << ", " << blocks_.size() << " blocks + "
         << terminals_.size() << " terminals, "
         << nets_.size() << " nets, alpha = " << alpha_ << endl;

    // init lambda proportional to outline area / total block area
    long long totalBlkArea = 0;
    for (auto& b : blocks_) totalBlkArea += (long long)b.w * b.h;
    lambda_ = max(1.0, (double)outlineW_ * outlineH_ / max(1LL, totalBlkArea));

    run(280.0);

    writeOutput(outFile);

    int       cw   = chipW(blocks_), ch = chipH(blocks_);
    long long hpwl = calcHPWL();
    long long cost = calcTrueCost(cw, ch, hpwl);
    cout << "[Final] cost = " << cost
         << ", hpwl = "  << hpwl
         << ", area = "  << (long long)cw * ch
         << " (" << cw << " x " << ch << ")"
         << ", elapsed: " << elapsed() << " s" << endl;
}