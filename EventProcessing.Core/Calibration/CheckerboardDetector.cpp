// Calibration/ 하위 폴더 - MSVC PCH 미사용(.vcxproj: PrecompiledHeader=NotUsing).
#include "CheckerboardDetector.h"
#include "EdgeCheckerboardReconstructor.h"

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

    CheckerboardDetection CheckerboardDetector::DetectEventImages(const cv::Mat& polarityImage, const cv::Mat& eventCountImage,
                                                                  const CheckerboardConfig& config, bool thorough,
                                                                  cv::Mat* detectionImage)
    {
        // 경계 재구성을 먼저 시도한다. 움직이는 보드에서는 polarity 이미지 검출이 거의 항상 실패하는데
        // 그 시도(특히 thorough)가 비싸기 때문이다. 깜빡이는 패턴처럼 칸 전체에 event가 있으면 칸 후보가
        // 생기지 않아 재구성이 곧바로 실패하고, 아래 polarity 이미지 검출로 넘어간다.
        if (!eventCountImage.empty())
        {
            // 라이브(thorough=false)에서는 큰 이미지면 절반 해상도로 먼저 시도한다(이 환경 측정 기준 약 5배 빠름).
            // 절반 해상도는 작은 칸/강한 노이즈/빠른 움직임에서 놓칠 수 있으므로, 실패하면 원 해상도로 다시 시도한다.
            // 코너는 원 해상도 좌표로 되돌린다(라이브 오버레이용; Capture는 thorough로 원 해상도에서 다시 검출).
            constexpr int kHalfResMinWidth = 1000;
            if (!thorough && eventCountImage.cols >= kHalfResMinWidth)
            {
                cv::Mat halfCount;
                cv::resize(eventCountImage, halfCount, cv::Size(), 0.5, 0.5, cv::INTER_AREA);
                const EdgeReconstruction halfRec = EdgeCheckerboardReconstructor::Reconstruct(halfCount);
                if (halfRec.ok)
                {
                    CheckerboardDetection halfDet = Detect(halfRec.image, config, /*thorough=*/false);
                    if (halfDet.found)
                    {
                        for (cv::Point2f& c : halfDet.corners)
                        {
                            c = (c + cv::Point2f(0.5f, 0.5f)) * 2.0f - cv::Point2f(0.5f, 0.5f);
                        }
                        halfDet.fromEdgeReconstruction = true;
                        if (detectionImage)
                        {
                            cv::resize(halfRec.image, *detectionImage, eventCountImage.size(), 0, 0, cv::INTER_NEAREST);
                        }
                        return halfDet;
                    }
                }
            }

            const EdgeReconstruction rec = EdgeCheckerboardReconstructor::Reconstruct(eventCountImage);
            if (rec.ok)
            {
                CheckerboardDetection edgeDet = Detect(rec.image, config, thorough);
                if (edgeDet.found)
                {
                    edgeDet.fromEdgeReconstruction = true;
                    if (detectionImage)
                    {
                        *detectionImage = rec.image;
                    }
                    return edgeDet;
                }
            }
        }

        if (detectionImage)
        {
            *detectionImage = polarityImage;
        }
        return Detect(polarityImage, config, thorough);
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
