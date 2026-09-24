#include "pch.h"
#include "CameraCalibrator.h"

namespace eventcore
{
    CalibrationResult CameraCalibrator::Calibrate(const std::vector<CalibrationObservation>& observations,
                                                  const cv::Size& imageSize,
                                                  const CheckerboardConfig& checkerboard,
                                                  CalibrationModel model)
    {
        CalibrationResult result;
        result.checkerboard = checkerboard;
        result.model = model;
        result.imageWidth = imageSize.width;
        result.imageHeight = imageSize.height;

        if (imageSize.width <= 0 || imageSize.height <= 0)
        {
            result.message = "Invalid image size";
            return result;
        }

        // object/image 대응쌍을 유효한 것만 모은다.
        std::vector<std::vector<cv::Point3f>> objectPoints;
        std::vector<std::vector<cv::Point2f>> imagePoints;
        objectPoints.reserve(observations.size());
        imagePoints.reserve(observations.size());

        for (const CalibrationObservation& obs : observations)
        {
            if (obs.objectPoints.empty() || obs.objectPoints.size() != obs.imagePoints.size())
            {
                continue;
            }
            objectPoints.push_back(obs.objectPoints);
            imagePoints.push_back(obs.imagePoints);
        }

        result.numObservations = static_cast<int>(objectPoints.size());

        // 평면 패턴 calibration의 기술적 최소(OpenCV가 풀 수 있는 최소). 정확도를 위해서는 더 많이 권장.
        constexpr size_t kMinViews = 3;
        if (objectPoints.size() < kMinViews)
        {
            result.message = "Need at least 3 valid observations to calibrate";
            return result;
        }

        cv::Mat cameraMatrix = cv::Mat::eye(3, 3, CV_64F);
        cv::Mat distCoeffs = cv::Mat::zeros(5, 1, CV_64F);  // k1, k2, p1, p2, k3
        std::vector<cv::Mat> rvecs;
        std::vector<cv::Mat> tvecs;

        try
        {
            // 일반 pinhole 모델. flags=0: 5개 distortion 계수(k1,k2,p1,p2,k3)를 모두 추정한다.
            const double rms = cv::calibrateCamera(
                objectPoints, imagePoints, imageSize, cameraMatrix, distCoeffs, rvecs, tvecs, 0);

            result.success = true;
            result.cameraMatrix = cameraMatrix;
            result.distCoeffs = distCoeffs;
            result.rmsReprojectionError = rms;
            result.rvecs = std::move(rvecs);
            result.tvecs = std::move(tvecs);
            result.message = "OK";
        }
        catch (const cv::Exception& ex)
        {
            result.success = false;
            result.message = std::string("cv::calibrateCamera failed: ") + ex.what();
        }

        return result;
    }
}
