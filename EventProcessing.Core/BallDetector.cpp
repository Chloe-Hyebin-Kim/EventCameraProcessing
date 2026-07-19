#include "pch.h"
#include "BallDetector.h"



namespace eventcore
{
    BallDetectionResult BallDetector::Detect(const cv::Mat& binaryImage)
    {
        BallDetectionResult result;

        if (binaryImage.empty())
        {
            return result;
        }

        std::vector<std::vector<cv::Point>> contours;

        cv::findContours(
            binaryImage,
            contours,
            cv::RETR_EXTERNAL,
            cv::CHAIN_APPROX_SIMPLE
        );

        if (contours.empty())
        {
            return result;
        }

        int bestIdx = -1;
        double bestArea = 0.0;

        for (int i = 0; i < static_cast<int>(contours.size()); ++i)
        {
            double area = cv::contourArea(contours[i]);

            if (area > bestArea)
            {
                bestArea = area;
                bestIdx = i;
            }
        }

        if (bestIdx < 0 || bestArea < 20.0)
        {
            return result;
        }

        cv::Point2f center;
        float radius = 0.0f;

        cv::minEnclosingCircle(contours[bestIdx], center, radius);

        result.detected = true;
        result.center = center;
        result.radius = radius;
        result.area = bestArea;
        result.boundingBox = cv::boundingRect(contours[bestIdx]);

        return result;
    }
}