#include "timemgr.h"
#include <algorithm>

void TimeManager::update(uint32_t remainingTime, uint32_t increment) {
    _remainingTime = remainingTime;
    _increment = increment;
}

uint32_t TimeManager::spareTime(int ply) {
    if (_remainingTime <= 500) {
        return _increment;
    }
    if (ply >= 35) {
        if (_remainingTime <= 100) {
            return _increment / 2;
        } else {
            return std::max(20u, _remainingTime / 4);
        }
    } else {
        return std::max(_increment - 1,
                        _remainingTime / (35 - ply) + _increment);
    }
}