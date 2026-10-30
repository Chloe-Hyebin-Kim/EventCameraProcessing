#pragma once

#include <vector>
#include <opencv2/opencv.hpp>

#include "../Event.h"             // lli
#include "CalibrationTypes.h"     // CheckerboardConfig
#include "CheckerboardDetector.h" // CheckerboardDetection

namespace eventcore
{
    // 하나의 checkerboard pose에 대한 calibration observation.
    // intrinsic calibration(cv::calibrateCamera)이 요구하는 3D<->2D 대응쌍을 담는다.
    struct CalibrationObservation
    {
        // board 로컬 좌표계의 3D 코너(Z=0 평면). 단위는 squareSizeMm(밀리미터).
        std::vector<cv::Point3f> objectPoints;
        // 이미지에서 검출된 코너 좌표 (u, v). objectPoints와 같은 순서/개수.
        std::vector<cv::Point2f> imagePoints;
        // 이 observation이 캡처된 시각(µs).
        lli timestampUs = 0;
    };

    // config로부터 checkerboard 3D 코너 좌표를 생성한다(row-major, Z=0).
    // 순서는 OpenCV findChessboardCorners* 가 돌려주는 코너 순서와 일치한다:
    //   for r in [0, rows): for c in [0, cols): (c*square, r*square, 0)
    std::vector<cv::Point3f> BuildObjectPoints(const CheckerboardConfig& config);

    // 여러 pose의 성공 검출을 calibration observation으로 모으는 수집기.
    // 하나의 수집기는 하나의 CheckerboardConfig만 사용한다(첫 샘플에서 고정). 다른 설정으로
    // 추가하려 하면 실패한다(먼저 Clear()가 필요) - 서로 다른 보드를 섞으면 calibration이 무의미하므로.
    class CalibrationSampleCollector
    {
    public:
        // det.found여야 하고 det.corners 개수가 config의 (rows*cols)와 같아야 한다.
        // count>0인데 config가 기존과 다르면 false. 성공 시 observation을 추가하고 true.
        // imageSize는 이 observation이 나온 calibration 이미지 크기(센서 해상도). 첫 샘플에서 고정된다.
        bool AddSample(const CheckerboardConfig& config, const CheckerboardDetection& detection,
                       lli timestampUs, const cv::Size& imageSize);

        bool RemoveLast();
        void Clear();

        size_t Count() const { return m_observations.size(); }
        bool Empty() const { return m_observations.empty(); }

        const std::vector<CalibrationObservation>& Observations() const { return m_observations; }
        // 유효 샘플이 있을 때 이 수집기가 사용 중인 체커보드 설정(Empty()면 의미 없음).
        const CheckerboardConfig& Config() const { return m_config; }
        // 유효 샘플이 있을 때 calibration 이미지 크기(Empty()면 0x0).
        const cv::Size& ImageSize() const { return m_imageSize; }

    private:
        std::vector<CalibrationObservation> m_observations;
        CheckerboardConfig m_config;
        cv::Size m_imageSize;
    };
}
