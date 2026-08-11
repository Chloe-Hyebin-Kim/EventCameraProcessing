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
        // hasPreviousBallCenter가 true면 이전 프레임에서 검출된 공 중심점에 가까운 컨투어를
        // 우선 선택해(BallDetector::DetectTracked) 추적 안정성을 높인다. 첫 프레임이거나 이전
        // 검출이 없으면 false로 호출한다 (BallDetector::Detect와 동일하게 최대 컨투어로 폴백).
        static EventProcessingResult Process(const std::vector<Event>& events, int width, int height, lli startUs, lli windowUs,
            bool hasPreviousBallCenter = false, const cv::Point2f& previousBallCenter = cv::Point2f(0.0f, 0.0f));
    };
}