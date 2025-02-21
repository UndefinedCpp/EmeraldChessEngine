#include "timemgr.h"
#include <algorithm>
#include <cmath>

void TimeManager::update(uint32_t remainingTime, uint32_t increment) {
    _remainingTime = remainingTime;
    _increment = increment;
}

uint32_t TimeManager::spareTime(int ply) {
    // Low time handling
    if (_remainingTime <= 500) {
        return _increment * 2 / 3;
    }
    // The following formula is searched
    //         Ax^2     1 - √x
    //   r% = ------ + --------
    //         x+B      Cx + D
    // where
    //   A = 0.0030980646
    //   B = 11.289064
    //   C = 8.76403
    //   D = -533.8745
    if (ply <= 55) {
        constexpr float A = 0.0030980646;
        constexpr float B = 11.289064;
        constexpr float C = 8.76403;
        constexpr float D = -533.8745;
        float ratio = (A * ply * ply) / (ply + B) +
                      (1 - std::sqrt(ply)) / (C * ply + D) + 0.01;
        float t = _remainingTime * ratio;
        int time = std::clamp((int)t, (int)_increment / 2,
                              (int)(_remainingTime - _increment / 2));
        return time;
    } else {
        return _remainingTime / 2;
    }
}