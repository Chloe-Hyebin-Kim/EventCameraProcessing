// Calibration/ 하위 폴더 - MSVC PCH 미사용(.vcxproj: PrecompiledHeader=NotUsing).
#include "CalibrationObservation.h"

#include <cmath>
#include <utility>

namespace eventcore
{
    std::vector<cv::Point3f> BuildObjectPoints(const CheckerboardConfig& config)
    {
        std::vector<cv::Point3f> points;

        const int rows = config.innerCornerRows;
        const int cols = config.innerCornerCols;
        if (rows < 2 || cols < 2)
        {
            return points;
        }

        points.reserve(static_cast<size_t>(rows) * static_cast<size_t>(cols));
        for (int r = 0; r < rows; ++r)
        {
            for (int c = 0; c < cols; ++c)
            {
                points.emplace_back(
                    static_cast<float>(c * config.squareSizeMm),
                    static_cast<float>(r * config.squareSizeMm),
                    0.0f);
            }
        }

        return points;
    }

    namespace
    {
        bool SameConfig(const CheckerboardConfig& a, const CheckerboardConfig& b)
        {
            return a.innerCornerRows == b.innerCornerRows
                && a.innerCornerCols == b.innerCornerCols
                && std::abs(a.squareSizeMm - b.squareSizeMm) < 1e-9;
        }
    }

    bool CalibrationSampleCollector::AddSample(const CheckerboardConfig& config, const CheckerboardDetection& detection, lli timestampUs, const cv::Size& imageSize)
    {
        if (!detection.found)
        {
            return false;
        }
        if (config.innerCornerRows < 2 || config.innerCornerCols < 2)
        {
            return false;
        }

        const size_t expected =
            static_cast<size_t>(config.innerCornerRows) * static_cast<size_t>(config.innerCornerCols);
        if (detection.corners.size() != expected)
        {
            return false;
        }

        // 서로 다른 보드 설정을 한 수집기에 섞지 않는다.
        if (!m_observations.empty() && !SameConfig(config, m_config))
        {
            return false;
        }

        CalibrationObservation obs;
        obs.objectPoints = BuildObjectPoints(config);
        obs.imagePoints = detection.corners;
        obs.timestampUs = timestampUs;

        if (obs.objectPoints.size() != obs.imagePoints.size())
        {
            return false;  // 방어적: object/image 대응쌍 개수가 어긋나면 추가하지 않는다.
        }

        if (m_observations.empty())
        {
            m_config = config;
            m_imageSize = imageSize;
        }
        m_observations.push_back(std::move(obs));
        return true;
    }

    bool CalibrationSampleCollector::RemoveLast()
    {
        if (m_observations.empty())
        {
            return false;
        }
        m_observations.pop_back();
        return true;
    }

    void CalibrationSampleCollector::Clear()
    {
        m_observations.clear();
    }
}
