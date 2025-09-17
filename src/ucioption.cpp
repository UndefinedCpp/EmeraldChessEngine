#include "ucioption.h"

UCIOption g_ucioption;

void UCIOption::set(const std::string& name, const std::string& value) {
    if (name == "Hash") {
        int parsedValue = std::stoi(value);
        if (parsedValue < 1 || parsedValue > 2048) {
            std::cout << "Value out of range" << std::endl;
        } else {
            hash.value = parsedValue;
            tt.init(parsedValue * 1024 * 1024 / sizeof(TTEntry));
        }
    }
}

std::ostream& operator<<(std::ostream& os, const UCIOption& option) {
    os << "option name Hash type spin default 16 min 1 max 2048" << std::endl;
    return os;
}