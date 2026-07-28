#pragma once

#include <vector>
#include <opencv2/opencv.hpp>

#include "Event.h"
#include "BallDetector.h"

namespace eventcore
{
    struct EventProcessingResult
    {
        cv::Mat positiveImage;
        cv::Mat negativeImage;
        cv::Mat mergedImage;
        cv::Mat binaryMask;
        cv::Mat debugImage;

        BallDetectionResult ball;
    };

    class EventProcessor
    {
    public:
        static EventProcessingResult Process(const std::vector<Event>& events, int width, int height, lli startUs, lli windowUs);
    };
}