#pragma once

#include "position.h"
#include "search.h"
#include <thread>

class Worker {
private:
    Position pos;
    // Search

public:
    Worker(const Position &pos);
    ~Worker();

    void start();
    void stop();
};