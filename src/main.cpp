#include "tt.h"
#include "uci.h"
#include <iostream>

using namespace std;

int main() {
    cout << "Emerald Chess Engine by UndefinedCpp, version " << ENGINE_VERSION
         << endl;
    tt.init(8 * 1024 * 1024); // todo refactor, currently fixed at 128MB

    std::string input;
    while (std::getline(std::cin, input)) {
        if (input == "quit") {
            break;
        } else {
            uci::execute(input);
        }
    }
    return 0;
}