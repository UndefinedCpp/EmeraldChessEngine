#pragma once

#include <stdint.h>

class TimeManager {
  private:
    uint32_t _remainingTime;
    uint32_t _increment;

  public:
    TimeManager() {
    }
    ~TimeManager() {
    }

  public:
    void update(uint32_t remainingTime, uint32_t increment);
    uint32_t spareTime(int ply);
};