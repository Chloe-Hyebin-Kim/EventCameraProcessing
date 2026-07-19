#pragma once

#include <opencv2/opencv.hpp>

namespace eventcore
{
    struct BallDetectionResult
    {
        bool detected = false;
        cv::Point2f center = cv::Point2f(0.0f, 0.0f);
        float radius = 0.0f;
        double area = 0.0;
        cv::Rect boundingBox;
    };

    class BallDetector
    {
    public:
        static BallDetectionResult Detect(const cv::Mat& binaryImage);
    };
}