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
        // detectBall이 false면 noise 필터/BallDetector/볼 오버레이를 건너뛴다(누적 이미지는 그대로
        // 생성). Calibration처럼 볼 검출이 필요 없는 경로에서 불필요한 검출을 끄는 데 쓴다.
        // 기존 호출부는 detectBall 기본값(true) 그대로라 동작이 바뀌지 않는다.
        static EventProcessingResult Process(const std::vector<Event>& events, int width, int height, lli startUs, lli windowUs, bool detectBall = true);
    };
}