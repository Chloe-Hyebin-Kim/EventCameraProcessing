#pragma once

#include <vector>
#include <opencv2/opencv.hpp>

#include "TypeDef.h"

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