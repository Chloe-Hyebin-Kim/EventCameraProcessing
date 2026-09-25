// Calibration/ 하위 폴더 - MSVC PCH 미사용(.vcxproj: PrecompiledHeader=NotUsing).
#include "CheckerboardDetector.h"

namespace eventcore
{
    cv::Size CheckerboardDetector::PatternSize(const CheckerboardConfig& config)
    {
        // OpenCV patternSize = Size(points_per_row, points_per_column) = Size(가로 코너, 세로 코너).
        return cv::Size(config.innerCornerCols, config.innerCornerRows);
    }

    CheckerboardDetection CheckerboardDetector::Detect(const cv::Mat& image, const CheckerboardConfig& config, bool thorough)
    {
        CheckerboardDetection det;

        if (image.empty())
        {
            return det;
        }

        const cv::Size pattern = PatternSize(config);
        if (pattern.width < 2 || pattern.height < 2)
        {
            return det;  // 내부 코너가 각 방향 2개 미만이면 체스판으로 볼 수 없다.
        }

        // grayscale 8U 보장(이벤트 누적 이미지는 보통 CV_8UC1이지만 방어적으로 변환).
        cv::Mat gray;
        if (image.type() == CV_8UC1)
        {
            gray = image;
        }
        else if (image.channels() == 3)
        {
            cv::cvtColor(image, gray, cv::COLOR_BGR2GRAY);
        }
        else
        {
            image.convertTo(gray, CV_8UC1);
        }

        std::vector<cv::Point2f> corners;
        bool found = false;

        // 1) sector-based 검출(선호). subpixel 보정 내장.
        //    thorough일 때만 EXHAUSTIVE|ACCURACY를 켠다(정확도↑, 속도↓). 라이브 오버레이(=false)에서는
        //    NORMALIZE만 써서 프레임마다 도는 비용을 최소화한다.
        int sbFlags = cv::CALIB_CB_NORMALIZE_IMAGE;
        if (thorough)
        {
            sbFlags |= cv::CALIB_CB_EXHAUSTIVE | cv::CALIB_CB_ACCURACY;
        }

        try
        {
            found = cv::findChessboardCornersSB(gray, pattern, corners, sbFlags);
        }
        catch (const cv::Exception&)
        {
            found = false;
            corners.clear();
        }

        // 2) 폴백: 고전적 검출 + subpixel 보정. thorough일 때만 수행한다
        //    (라이브 경로에서 미검출 프레임마다 이 무거운 폴백까지 도는 것을 막는다).
        if (!found && thorough)
        {
            corners.clear();
            bool classicFound = false;
            try
            {
                classicFound = cv::findChessboardCorners(
                    gray, pattern, corners,
                    cv::CALIB_CB_ADAPTIVE_THRESH | cv::CALIB_CB_NORMALIZE_IMAGE);
            }
            catch (const cv::Exception&)
            {
                classicFound = false;
                corners.clear();
            }

            if (classicFound && !corners.empty())
            {
                try
                {
                    cv::cornerSubPix(
                        gray, corners, cv::Size(11, 11), cv::Size(-1, -1),
                        cv::TermCriteria(cv::TermCriteria::EPS + cv::TermCriteria::MAX_ITER, 30, 0.1));
                }
                catch (const cv::Exception&)
                {
                    // subpixel 보정 실패해도 검출 자체는 유효한 것으로 둔다.
                }
                found = true;
            }
        }

        det.found = found;
        if (found)
        {
            det.corners = std::move(corners);
        }

        return det;
    }

    void CheckerboardDetector::DrawCorners(cv::Mat& bgrImage, const CheckerboardConfig& config, const CheckerboardDetection& detection)
    {
        if (bgrImage.empty() || detection.corners.empty())
        {
            return;
        }

        cv::drawChessboardCorners(bgrImage, PatternSize(config), cv::Mat(detection.corners), detection.found);
    }
}
