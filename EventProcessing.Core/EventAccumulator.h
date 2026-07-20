#pragma once


#include "opencv.hpp"
#include "Event.h"
#include <vector>

namespace eventcore
{
    class EventAccumulator
    {
    public:
        static void Accumulate(const vector<Event>& events, int width, int height, lli startUs, lli windowUs, cv::Mat& positiveImage, cv::Mat& negativeImage, cv::Mat& mergedImage);

    private:
        static cv::Mat NormalizeTo8U(const cv::Mat& src);
    };
}