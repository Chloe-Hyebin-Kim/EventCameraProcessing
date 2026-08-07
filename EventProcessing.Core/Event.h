#pragma once

#include <cstdint>

//using namespace std;
//#define lli int64_t

#define cvBLUE cv::Scalar(255, 0, 0)
#define cvRED cv::Scalar(0, 0, 255)
#define cvGREEN cv::Scalar(0, 255, 0)

//(EVK4 HD / IMX636)
//1280��720 pixel CMOS vision sensor
#define WIDTH 1280
#define HEIGHT 720

namespace eventcore
{
    using lli = std::int64_t;

    struct Event
    {
        lli t_us = 0;   // timestamp in microseconds
        int x = 0;
        int y = 0;
        int polarity = 0;  // +1 or -1
    };
}