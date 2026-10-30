#pragma once

#include <vector>
#include <opencv2/opencv.hpp>

#include "CalibrationTypes.h"

namespace eventcore
{
    // 한 장의 calibration 이미지에서 checkerboard(체스판) 내부 코너를 검출한 결과.
    struct CheckerboardDetection
    {
        bool found = false;
        // found == true일 때 innerCornerCols * innerCornerRows 개의 image 좌표 (u, v).
        // OpenCV 관례상 행 우선(row-major) 순서로 정렬되어 있다.
        std::vector<cv::Point2f> corners;
        // true면 polarity 이미지가 아니라 EdgeCheckerboardReconstructor가 재구성한 이미지에서 찾은 결과.
        bool fromEdgeReconstruction = false;
    };

    // Calibration 이미지(폴라리티 누적 이미지 등)에서 checkerboard 코너를 검출한다.
    // OpenCV calib3d/imgproc만 사용하며 Metavision/Qt에 의존하지 않는다(카메라 입력과 분리).
    class CheckerboardDetector
    {
    public:
        // image: CV_8UC1(권장) 또는 3채널/기타. 내부에서 grayscale로 맞춘다.
        // 성공 시 subpixel 정밀도로 보정된 코너를 담아 반환한다.
        //
        // 검출 전략(이벤트 누적 이미지는 저대비/노이즈/불균일 조명이 흔하다는 점을 고려):
        //  1) findChessboardCornersSB(sector-based)를 먼저 시도한다. blur/저대비에 상대적으로
        //     강하고 subpixel 보정이 내장되어 있어, 이벤트 누적 이미지에 유리할 것으로 기대된다.
        //  2) 실패하면 고전적 findChessboardCorners + cornerSubPix로 폴백한다.
        // 두 방법 중 무엇이 실제 EVK4HD 데이터에서 더 안정적인지는 실촬영으로 검증해야 하며,
        // 그 결과에 따라 순서/플래그를 조정할 수 있다.
        //
        // thorough:
        //  - true(기본): SB에 EXHAUSTIVE|ACCURACY 플래그를 주고, 실패 시 고전 검출로 폴백한다.
        //    검출률이 높지만 느리다(1280x720에서 특히 미검출 프레임이 비쌈). 저장(Capture)처럼
        //    사용자가 한 번만 트리거하는 정밀 검출에 쓴다.
        //  - false: 가벼운 SB(NORMALIZE만)만 시도하고 고전 폴백을 하지 않는다. 훨씬 빨라
        //    라이브 프리뷰 오버레이처럼 매 프레임 도는 경로에 적합하다.
        static CheckerboardDetection Detect(const cv::Mat& image, const CheckerboardConfig& config, bool thorough = true);

        // Event 누적 결과에서 체커보드를 찾는다.
        //  1) eventCountImage가 있으면, 움직이는 체커보드의 경계 띠로부터 칸을 재구성
        //     (EdgeCheckerboardReconstructor)한 이미지에서 Detect()를 시도한다.
        //  2) 실패하면 polarityImage(CalibrationImageBuilder::Image())에서 Detect()를 시도한다. 깜빡이는 화면
        //     패턴처럼 칸 전체가 채워진 경우 여기서 찾힌다(이때 1)의 재구성은 칸 후보가 없어 금방 끝난다).
        // detectionImage가 주어지면 실제로 검출에 사용한 이미지(재구성 시 재구성 이미지)를 담는다.
        static CheckerboardDetection DetectEventImages(const cv::Mat& polarityImage, const cv::Mat& eventCountImage,
                                                       const CheckerboardConfig& config, bool thorough,
                                                       cv::Mat* detectionImage = nullptr);

        // bgrImage(CV_8UC3) 위에 검출 결과를 오버레이한다(cv::drawChessboardCorners).
        // found == false여도 부분적으로 찾은 후보 코너가 있으면 표시해 디버깅에 도움을 준다.
        static void DrawCorners(cv::Mat& bgrImage, const CheckerboardConfig& config, const CheckerboardDetection& detection);

        // config로부터 OpenCV patternSize = cv::Size(innerCornerCols, innerCornerRows)를 만든다.
        static cv::Size PatternSize(const CheckerboardConfig& config);
    };
}
