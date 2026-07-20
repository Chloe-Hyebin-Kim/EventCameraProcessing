#pragma once

#include <cstdint>
//#include "TypeDef.h"

#define lli int64_t

#define cvBLUE cv::Scalar(255, 0, 0)
#define cvRED cv::Scalar(0, 255, 0)
#define cvGREEN cv::Scalar(0, 0, 255)

namespace eventcore
{
    struct Event
    {
        lli t_us = 0;   // timestamp in microseconds
        int x = 0;
        int y = 0;
        int polarity = 0;  // +1 or -1
    };
}