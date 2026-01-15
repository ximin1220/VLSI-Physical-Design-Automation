#include <iostream>
#include "router.h"
using namespace std;

int main(int argc, char* argv[]) {
    if (argc < 3) {
        cout << "Usage: ./hw4 <input_file> <output_file>" << endl;
        return 1;
    }

    GlobalRouter router;
    router.parseInput(argv[1]);
    router.runRouting();
    router.writeOutput(argv[2]);

    return 0;
}
