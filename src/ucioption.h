#pragma once

#include "tt.h"
#include "types.h"
#include <map>
#include <ostream>

class UCIOption {
public:
    UCIOption() = default;

    void set(const std::string& name, const std::string& value);

private:
    struct Numeric {
        int value;
        int min;
        int max;
        Numeric(int value, int min, int max) : value(value), min(min), max(max) {}
    };

    Numeric hash = Numeric(16, 1, 2048);
};

std::ostream& operator<<(std::ostream& os, const UCIOption& option);

extern UCIOption g_ucioption;
