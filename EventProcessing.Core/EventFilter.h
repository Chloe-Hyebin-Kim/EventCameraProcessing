#pragma once

#include <opencv2/opencv.hpp>

namespace eventcore
{
    class EventFilter
    {
    public:
        static cv::Mat RemoveSmallNoise(const cv::Mat& input);
    };
}