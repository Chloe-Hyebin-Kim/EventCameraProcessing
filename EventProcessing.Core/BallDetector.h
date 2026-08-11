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
        // 가장 큰 컨투어를 공으로 선택. 노이즈가 있는 실데이터에서는 신발/클럽 등
        // 엉뚱한 큰 컨투어로 튀기 쉽다 (README의 "Known limitation" 참고).
        static BallDetectionResult Detect(const cv::Mat& binaryImage);

        // hasPreviousCenter가 true면, minArea 이상인 후보들 중 previousCenter에 가장 가까운
        // 컨투어를 우선 선택한다 (같은 물체를 계속 추적한다고 가정). 후보가 없거나
        // hasPreviousCenter가 false면 Detect()와 동일하게 가장 큰 컨투어로 폴백한다.
        //
        // 실제 골프 스윙 RAW 녹화(7iron_woman)로 측정한 결과, 이 방식이 프레임간 중심점
        // 이동거리를 크게 줄인다 (평균 102px -> 47px, p90 291px -> 120px, minArea=500 기준).
        static BallDetectionResult DetectTracked(const cv::Mat& binaryImage, bool hasPreviousCenter, const cv::Point2f& previousCenter, double minArea = 500.0);
    };
}