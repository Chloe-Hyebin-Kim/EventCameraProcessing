#pragma once

#include "BallDetector.h"
#include "Event.h"

#include <opencv2/opencv.hpp>
#include <vector>

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
        static EventProcessingResult Process(const vector<Event>& events, int width, int height, lli startUs, lli windowUs);
    };
}