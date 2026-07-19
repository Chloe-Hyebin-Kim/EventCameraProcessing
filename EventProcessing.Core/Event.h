#pragma once

#include <cstdint>

namespace eventcore
{
    struct Event
    {
        int64_t t_us = 0;   // timestamp in microseconds
        int x = 0;
        int y = 0;
        int polarity = 0;  // +1 or -1
    };
}