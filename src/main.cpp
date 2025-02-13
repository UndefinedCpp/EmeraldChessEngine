#include "tt.h"
#include "uci.h"
#include <iostream>

using namespace std;

int main() {
    cout << "Emerald Chess Engine by UndefinedCpp, indev version" << endl;
    tt.init(1024 * 1024); // temporary initialize transposition table

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