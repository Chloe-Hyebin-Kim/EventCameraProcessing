#pragma once


#include "opencv.hpp"
#include "Event.h"
#include <vector>

namespace eventcore
{
    class EventAccumulator
    {
    public:
        static void Accumulate(
            const std::vector<Event>& events,
            int width,
            int height,
            int64_t startUs,
            int64_t windowUs,
            cv::Mat& positiveImage,
            cv::Mat& negativeImage,
            cv::Mat& mergedImage
        );

    private:
        static cv::Mat NormalizeTo8U(const cv::Mat& src);
    };
}