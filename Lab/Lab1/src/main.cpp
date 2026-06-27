#include "stitcher.h"
#include <iostream>
#include <string>
#include <fstream>

using namespace std;

int main(int argc, char *argv[])
{
    if (argc < 3)
    {
        cerr << "Usage: ./bin/cstitch <input>.txt <output>.txt" << endl;
        return 1;
    }

    try
    {
        string inputFile = argv[1];
        string outputFile = argv[2];

        ifstream infile(inputFile);
        if (!infile)
        {
            cerr << "Failed to open input file: " << inputFile << endl;
            return 1;
        }

        ofstream outfile(outputFile);
        if (!outfile)
        {
            cerr << "Failed to open output file: " << outputFile << endl;
            return 1;
        }

        CornerStitching stitcher;
        stitcher.solve(inputFile, outputFile);


        return 0;
    }
    catch (const exception &e)
    {
        cerr << "Error: " << e.what() << endl;
        return 1;
    }
}