#pragma once

#include <vector>
#include <opencv2/opencv.hpp>

#include "CalibrationTypes.h"
#include "CalibrationObservation.h"

namespace eventcore
{
    // 사용 distortion 모델. 기본은 일반 pinhole(radial k1,k2,k3 + tangential p1,p2).
    // Fisheye는 실제 렌즈가 초광각/어안이고 pinhole로 재투영 오차가 크게 남는 것이 확인될 때만
    // 근거를 가지고 선택한다(현재는 Pinhole만 구현; Fisheye는 향후 확장 지점).
    enum class CalibrationModel
    {
        Pinhole,
    };

    // Intrinsic calibration 결과.
    struct CalibrationResult
    {
        bool success = false;
        std::string message;  // 실패 원인 또는 참고 메시지(사람이 읽는 용도)

        int imageWidth = 0;
        int imageHeight = 0;

        cv::Mat cameraMatrix;  // 3x3 CV_64F: [fx 0 cx; 0 fy cy; 0 0 1]
        cv::Mat distCoeffs;    // (k1, k2, p1, p2, k3)

        double rmsReprojectionError = 0.0;  // cv::calibrateCamera가 돌려주는 전체 RMS(px)
        int numObservations = 0;            // 실제 calibration에 사용된 observation 수

        CheckerboardConfig checkerboard;
        CalibrationModel model = CalibrationModel::Pinhole;

        // per-view 외부 파라미터(Phase 5의 per-observation 재투영 오차 계산에 사용).
        std::vector<cv::Mat> rvecs;
        std::vector<cv::Mat> tvecs;

        // 편의 접근자(cameraMatrix가 비어 있으면 0을 돌려준다).
        double fx() const { return cameraMatrix.empty() ? 0.0 : cameraMatrix.at<double>(0, 0); }
        double fy() const { return cameraMatrix.empty() ? 0.0 : cameraMatrix.at<double>(1, 1); }
        double cx() const { return cameraMatrix.empty() ? 0.0 : cameraMatrix.at<double>(0, 2); }
        double cy() const { return cameraMatrix.empty() ? 0.0 : cameraMatrix.at<double>(1, 2); }
        double dist(int i) const
        {
            return (distCoeffs.empty() || i < 0 || i >= static_cast<int>(distCoeffs.total()))
                ? 0.0 : distCoeffs.at<double>(i);
        }
    };

    class CameraCalibrator
    {
    public:
        // observations를 cv::calibrateCamera로 처리해 K와 distortion을 계산한다.
        // - imageSize: calibration 이미지(센서) 크기.
        // - 최소 3개 이상의 유효 observation이 필요하다(평면 패턴 calibration의 기술적 최소).
        //   정확도를 위해서는 훨씬 많은(서로 다른 pose의) observation이 권장된다.
        // 실패 시 result.success == false, result.message에 원인이 담긴다.
        static CalibrationResult Calibrate(const std::vector<CalibrationObservation>& observations,
                                           const cv::Size& imageSize,
                                           const CheckerboardConfig& checkerboard,
                                           CalibrationModel model = CalibrationModel::Pinhole);
    };
}
