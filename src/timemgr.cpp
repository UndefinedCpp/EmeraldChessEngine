#include "timemgr.h"
#include <algorithm>
#include <cmath>

void TimeManager::update(uint32_t remainingTime, uint32_t increment) {
    _remainingTime = remainingTime;
    _increment = increment;
}

uint32_t TimeManager::spareTime(int ply) {
    if (_remainingTime <= 1000) {
        float ratio = _remainingTime / 1000.0f;
        return (_remainingTime + _increment) * ratio * ratio / 4.0;
    }
    if (ply >= 40) {
        if (_remainingTime <= 100) {
            return _increment / 2;
        } else {
            return std::max(20u, _remainingTime / 4);
        }
    } else {
        return std::max(_increment - 1,
                        _remainingTime / (40 - ply) + _increment);
    }
}