#include "pch.h"
#include "RobustBallDetector.h"

#include <algorithm>
#include <cmath>
#include <limits>

namespace eventcore
{
    namespace
    {
        struct Candidate
        {
            size_t index = 0;
            double area = 0.0;
            double perimeter = 0.0;
            double circularity = 0.0;
            double aspectRatio = 0.0;
            cv::Rect boundingBox;
            cv::Point2f momentCentroid = cv::Point2f(0.0f, 0.0f);
            CircleFitResult fit;
        };

        double Distance(const cv::Point2f& a, const cv::Point2f& b)
        {
            const double dx = static_cast<double>(a.x) - static_cast<double>(b.x);
            const double dy = static_cast<double>(a.y) - static_cast<double>(b.y);
            return std::sqrt(dx * dx + dy * dy);
        }
    }

    BallDetectionResult RobustBallResult::ToLegacy() const
    {
        BallDetectionResult legacy;

        legacy.detected = detected;
        legacy.center = center;
        legacy.radius = radius;
        legacy.area = area;
        legacy.boundingBox = boundingBox;

        return legacy;
    }

    const char* RobustBallDetector::RejectReasonName(BallRejectReason reason)
    {
        switch (reason)
        {
        case BallRejectReason::None:              return "none";
        case BallRejectReason::EmptyImage:        return "empty_image";
        case BallRejectReason::NoContours:        return "no_contours";
        case BallRejectReason::AllRejectedByGate: return "all_rejected_by_gate";
        case BallRejectReason::FitFailed:         return "fit_failed";
        }

        return "unknown";
    }

    RobustBallResult RobustBallDetector::Detect(const cv::Mat& binaryImage,
                                                const BallGate& gate,
                                                const CircleFitOptions& fitOptions)
    {
        RobustBallResult result;

        if (binaryImage.empty())
        {
            result.reject = BallRejectReason::EmptyImage;
            return result;
        }

        std::vector<std::vector<cv::Point>> contours;

        // CHAIN_APPROX_NONE: 원 적합에 쓸 윤곽 점이 필요하므로 근사하지 않고 전부 받는다.
        // (기존 코드의 CHAIN_APPROX_SIMPLE은 직선 구간의 중간 점을 버려서 적합 품질이 떨어진다)
        cv::findContours(binaryImage, contours, cv::RETR_EXTERNAL, cv::CHAIN_APPROX_NONE);

        result.contoursTotal = static_cast<int>(contours.size());

        if (contours.empty())
        {
            result.reject = BallRejectReason::NoContours;
            return result;
        }

        std::vector<Candidate> passed;
        passed.reserve(contours.size());

        for (size_t i = 0; i < contours.size(); ++i)
        {
            const std::vector<cv::Point>& contour = contours[i];

            if (contour.size() < static_cast<size_t>(fitOptions.minPoints))
            {
                continue;
            }

            Candidate c;
            c.index = i;
            c.area = cv::contourArea(contour);

            if (c.area < gate.minAreaPx)
            {
                continue;
            }

            if (gate.maxAreaPx > 0.0 && c.area > gate.maxAreaPx)
            {
                continue;
            }

            c.perimeter = cv::arcLength(contour, true);

            if (c.perimeter > 1e-9)
            {
                c.circularity = 4.0 * CV_PI * c.area / (c.perimeter * c.perimeter);
            }

            if (gate.minCircularity > 0.0 && c.circularity < gate.minCircularity)
            {
                continue;   // 클럽 샤프트/스윙 궤적 같은 가늘고 긴 형상은 여기서 걸린다
            }

            c.boundingBox = cv::boundingRect(contour);

            const double longSide = std::max(c.boundingBox.width, c.boundingBox.height);
            const double shortSide = std::max(1, std::min(c.boundingBox.width, c.boundingBox.height));
            c.aspectRatio = longSide / shortSide;

            if (gate.maxAspectRatio > 0.0 && c.aspectRatio > gate.maxAspectRatio)
            {
                continue;
            }

            c.fit = CircleFit::Fit(contour, fitOptions);

            if (!c.fit.ok)
            {
                continue;
            }

            if (c.fit.r < static_cast<double>(gate.minRadiusPx))
            {
                continue;
            }

            if (gate.maxRadiusPx > 0.0f && c.fit.r > static_cast<double>(gate.maxRadiusPx))
            {
                continue;   // 비행 중 긴 얼룩(반지름 수백 px)은 여기서 걸린다
            }

            const cv::Point2f fitCenter(static_cast<float>(c.fit.cx), static_cast<float>(c.fit.cy));

            if (gate.useTrackHint && gate.trackGatePx > 0.0f)
            {
                if (Distance(fitCenter, gate.trackHint) > static_cast<double>(gate.trackGatePx))
                {
                    continue;
                }
            }

            const cv::Moments m = cv::moments(contour);

            if (std::abs(m.m00) > 1e-9)
            {
                c.momentCentroid = cv::Point2f(static_cast<float>(m.m10 / m.m00),
                                               static_cast<float>(m.m01 / m.m00));
            }
            else
            {
                c.momentCentroid = fitCenter;
            }

            passed.push_back(c);
        }

        result.contoursPassed = static_cast<int>(passed.size());

        if (passed.empty())
        {
            result.reject = BallRejectReason::AllRejectedByGate;
            return result;
        }

        const Candidate* best = nullptr;

        if (gate.useTrackHint)
        {
            // 직전 위치에 가장 가까운 것을 고른다. 트래킹 중에는 이것이 면적보다 신뢰할 만하다.
            double bestDist = std::numeric_limits<double>::max();

            for (const Candidate& c : passed)
            {
                const cv::Point2f center(static_cast<float>(c.fit.cx), static_cast<float>(c.fit.cy));
                const double d = Distance(center, gate.trackHint);

                if (d < bestDist)
                {
                    bestDist = d;
                    best = &c;
                }
            }
        }
        else
        {
            double bestArea = -1.0;

            for (const Candidate& c : passed)
            {
                if (c.area > bestArea)
                {
                    bestArea = c.area;
                    best = &c;
                }
            }
        }

        if (best == nullptr)
        {
            result.reject = BallRejectReason::FitFailed;
            return result;
        }

        result.detected = true;
        result.center = cv::Point2f(static_cast<float>(best->fit.cx), static_cast<float>(best->fit.cy));
        result.radius = static_cast<float>(best->fit.r);
        result.momentCentroid = best->momentCentroid;
        result.area = best->area;
        result.perimeter = best->perimeter;
        result.circularity = best->circularity;
        result.aspectRatio = best->aspectRatio;
        result.boundingBox = best->boundingBox;
        result.fit = best->fit;
        result.radiusTrusted = best->fit.radiusTrusted;
        result.reject = BallRejectReason::None;

        return result;
    }
}
