#include "pch.h"
#include "CameraCalibrator.h"

#include <algorithm>
#include <cmath>

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
        std::vector<lli> usedTimestamps;
        objectPoints.reserve(observations.size());
        imagePoints.reserve(observations.size());
        usedTimestamps.reserve(observations.size());

        for (const CalibrationObservation& obs : observations)
        {
            if (obs.objectPoints.empty() || obs.objectPoints.size() != obs.imagePoints.size())
            {
                continue;
            }
            objectPoints.push_back(obs.objectPoints);
            imagePoints.push_back(obs.imagePoints);
            usedTimestamps.push_back(obs.timestampUs);
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
            result.rvecs = rvecs;
            result.tvecs = tvecs;
            result.viewTimestamps = usedTimestamps;
            result.message = "OK";

            // per-view 재투영 오차: 각 view의 3D 코너를 추정된 K/D/외부파라미터로 다시 투영해
            // 검출된 코너와의 RMS 거리를 구한다(projectPoints). 임계값으로 자동 제거하지 않는다.
            result.perViewErrors.resize(objectPoints.size(), 0.0);
            for (size_t i = 0; i < objectPoints.size(); ++i)
            {
                std::vector<cv::Point2f> projected;
                cv::projectPoints(objectPoints[i], rvecs[i], tvecs[i], cameraMatrix, distCoeffs, projected);

                double sumSq = 0.0;
                const size_t n = std::min(projected.size(), imagePoints[i].size());
                for (size_t j = 0; j < n; ++j)
                {
                    const double dx = static_cast<double>(projected[j].x) - imagePoints[i][j].x;
                    const double dy = static_cast<double>(projected[j].y) - imagePoints[i][j].y;
                    sumSq += dx * dx + dy * dy;
                }
                result.perViewErrors[i] = (n > 0) ? std::sqrt(sumSq / static_cast<double>(n)) : 0.0;
            }
        }
        catch (const cv::Exception& ex)
        {
            result.success = false;
            result.message = std::string("cv::calibrateCamera failed: ") + ex.what();
        }

        return result;
    }
}
