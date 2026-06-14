#include <iostream>
#include <string>
#include "floorplan.h"

using namespace std;

int main(int argc, char* argv[])
{
    if (argc != 5) {
        cerr << "Usage: ./Lab2 <alpha> <input.block> <input.nets> <output.rpt>" << endl;
        return 1;
    }

    try {
        double alpha      = stod(argv[1]);
        string input_blk  = argv[2];
        string input_net  = argv[3];
        string outputFile = argv[4];

        Floorplanner planner;
        planner.solve(input_blk, input_net, outputFile, alpha);
    }
    catch (const exception& e) {
        cerr << "Error: " << e.what() << endl;
        return 1;
    }

    return 0;
}