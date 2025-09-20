#include "annotate.h"
#include "tt.h"
#include "uci.h" // for ENGINE_VERSION

#include <iostream>

using namespace std;

int main(int argc, char* argv[]) {
    cout << "Emerald Chess Engine by UndefinedCpp, version " << ENGINE_VERSION << endl;

    g_ucioption.set("Hash", "16"); // default 16 MB

    // Parse command line
    if (argc > 1) {
        const std::string mode = argv[1];
        if (mode == "annotate") {
            if (argc != 3) {
                cout << "Usage: " << argv[0] << " annotate input_file" << endl;
                return 1;
            } else {
                annotate_main(argv[2]);
            }
        } else {
            cout << "Unrecognized mode: " << mode << endl;
            return 1;
        }
    }
    // Enter simple UCI mode
    else {
        std::string input;
        while (std::getline(std::cin, input)) {
            if (input == "quit") {
                break;
            } else {
                uci::execute(input);
            }
        }
    }

    return 0;
}