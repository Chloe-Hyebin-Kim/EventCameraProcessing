#include "pch.h"
#include "EventBallFinder.h"

#include <algorithm>
#include <cmath>
#include <limits>
#include <vector>

namespace eventcore
{
    namespace
    {
        double Distance(const cv::Point2f& a, const cv::Point2f& b)
        {
            const double dx = static_cast<double>(a.x) - static_cast<double>(b.x);
            const double dy = static_cast<double>(a.y) - static_cast<double>(b.y);
            return std::sqrt(dx * dx + dy * dy);
        }

        struct Cluster
        {
            std::vector<cv::Point2d> points;
            cv::Rect boundingBox;
            cv::Point2f centroid = cv::Point2f(0.0f, 0.0f);
            CircleFitResult fit;
            double aspectRatio = 0.0;
            double rmsRatio = 0.0;
        };
    }

    BallDetectionResult EventBallResult::ToLegacy() const
    {
        BallDetectionResult legacy;

        legacy.detected = detected;
        legacy.center = center;
        legacy.radius = radius;
        legacy.area = static_cast<double>(eventsUsed);   // 점군이라 면적 개념이 없다. 이벤트 수로 대체.
        legacy.boundingBox = boundingBox;

        return legacy;
    }

    EventBallResult EventBallFinder::Find(const std::vector<Event>& events,
                                          int width,
                                          int height,
                                          const BallGate& gate,
                                          const EventBallFinderConfig& config,
                                          const CircleFitOptions& fitOptions)
    {
        EventBallResult result;

        if (events.empty() || width <= 0 || height <= 0)
        {
            result.reject = BallRejectReason::EmptyImage;
            return result;
        }

        const int cell = std::max(1, config.cellPx);
        const int gw = (width + cell - 1) / cell;
        const int gh = (height + cell - 1) / cell;
        const std::size_t cells = static_cast<std::size_t>(gw) * static_cast<std::size_t>(gh);

        // 1) 밀도 격자
        std::vector<int> count(cells, 0);

        for (const Event& e : events)
        {
            if (e.x < 0 || e.x >= width || e.y < 0 || e.y >= height)
            {
                continue;
            }

            const int gx = e.x / cell;
            const int gy = e.y / cell;
            ++count[static_cast<std::size_t>(gy) * gw + gx];
        }

        // 2) 점유 셀 연결성분 (형태학 연산 없이 BFS)
        std::vector<int> label(cells, -1);
        std::vector<int> stack;
        int nLabels = 0;

        // 연결 반경 안의 상대 오프셋을 미리 만들어 둔다.
        const int link = std::max(1, config.linkRadiusCells);
        std::vector<int> offX;
        std::vector<int> offY;

        for (int oy = -link; oy <= link; ++oy)
        {
            for (int ox = -link; ox <= link; ++ox)
            {
                if (ox == 0 && oy == 0)
                {
                    continue;
                }

                offX.push_back(ox);
                offY.push_back(oy);
            }
        }

        const int neighbours = static_cast<int>(offX.size());

        for (std::size_t seed = 0; seed < cells; ++seed)
        {
            if (count[seed] < config.minEventsPerCell || label[seed] >= 0)
            {
                continue;
            }

            const int id = nLabels++;
            stack.clear();
            stack.push_back(static_cast<int>(seed));
            label[seed] = id;

            while (!stack.empty())
            {
                const int cur = stack.back();
                stack.pop_back();

                const int cx = cur % gw;
                const int cy = cur / gw;

                for (int k = 0; k < neighbours; ++k)
                {
                    const int nx = cx + offX[static_cast<std::size_t>(k)];
                    const int ny = cy + offY[static_cast<std::size_t>(k)];

                    if (nx < 0 || nx >= gw || ny < 0 || ny >= gh)
                    {
                        continue;
                    }

                    const std::size_t ni = static_cast<std::size_t>(ny) * gw + nx;

                    if (label[ni] >= 0 || count[ni] < config.minEventsPerCell)
                    {
                        continue;
                    }

                    label[ni] = id;
                    stack.push_back(static_cast<int>(ni));
                }
            }
        }

        result.clustersTotal = nLabels;

        if (nLabels == 0)
        {
            result.reject = BallRejectReason::NoContours;
            return result;
        }

        // 3) 이벤트를 클러스터별로 모은다
        std::vector<int> cellsPerLabel(static_cast<std::size_t>(nLabels), 0);

        for (std::size_t i = 0; i < cells; ++i)
        {
            if (label[i] >= 0)
            {
                ++cellsPerLabel[static_cast<std::size_t>(label[i])];
            }
        }

        std::vector<Cluster> clusters(static_cast<std::size_t>(nLabels));

        for (const Event& e : events)
        {
            if (e.x < 0 || e.x >= width || e.y < 0 || e.y >= height)
            {
                continue;
            }

            const int id = label[static_cast<std::size_t>(e.y / cell) * gw + (e.x / cell)];

            if (id < 0)
            {
                continue;
            }

            clusters[static_cast<std::size_t>(id)].points.emplace_back(
                static_cast<double>(e.x), static_cast<double>(e.y));
        }

        // 4) 클러스터마다 원 적합 + 게이트
        std::vector<const Cluster*> passed;

        for (int id = 0; id < nLabels; ++id)
        {
            Cluster& c = clusters[static_cast<std::size_t>(id)];

            if (cellsPerLabel[static_cast<std::size_t>(id)] < config.minCellsPerCluster)
            {
                continue;
            }

            if (static_cast<int>(c.points.size()) < config.minEventsPerCluster)
            {
                continue;
            }

            if (static_cast<int>(c.points.size()) < fitOptions.minPoints)
            {
                continue;
            }

            double minX = std::numeric_limits<double>::max();
            double maxX = -std::numeric_limits<double>::max();
            double minY = std::numeric_limits<double>::max();
            double maxY = -std::numeric_limits<double>::max();
            double sumX = 0.0;
            double sumY = 0.0;

            for (const cv::Point2d& p : c.points)
            {
                minX = std::min(minX, p.x);
                maxX = std::max(maxX, p.x);
                minY = std::min(minY, p.y);
                maxY = std::max(maxY, p.y);
                sumX += p.x;
                sumY += p.y;
            }

            const double n = static_cast<double>(c.points.size());
            c.centroid = cv::Point2f(static_cast<float>(sumX / n), static_cast<float>(sumY / n));

            c.boundingBox = cv::Rect(static_cast<int>(std::floor(minX)),
                                     static_cast<int>(std::floor(minY)),
                                     static_cast<int>(std::ceil(maxX - minX)) + 1,
                                     static_cast<int>(std::ceil(maxY - minY)) + 1);

            const double longSide = std::max(c.boundingBox.width, c.boundingBox.height);
            const double shortSide = std::max(1, std::min(c.boundingBox.width, c.boundingBox.height));
            c.aspectRatio = longSide / shortSide;

            if (gate.maxAspectRatio > 0.0 && c.aspectRatio > gate.maxAspectRatio)
            {
                continue;   // 클럽 샤프트/스윙 궤적
            }

            c.fit = CircleFit::Fit(c.points, fitOptions);

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
                continue;
            }

            c.rmsRatio = (c.fit.r > 1e-9) ? (c.fit.rmsPx / c.fit.r) : 1e9;

            if (config.maxFitRmsRatio > 0.0 && c.rmsRatio > config.maxFitRmsRatio)
            {
                continue;   // 링이 아니라 채워진 덩어리이거나 직선에 가깝다
            }

            if (gate.useTrackHint && gate.trackGatePx > 0.0f)
            {
                const cv::Point2f fitCenter(static_cast<float>(c.fit.cx), static_cast<float>(c.fit.cy));

                if (Distance(fitCenter, gate.trackHint) > static_cast<double>(gate.trackGatePx))
                {
                    continue;
                }
            }

            passed.push_back(&c);
        }

        result.clustersPassed = static_cast<int>(passed.size());

        if (passed.empty())
        {
            result.reject = BallRejectReason::AllRejectedByGate;
            return result;
        }

        const Cluster* best = nullptr;

        if (gate.useTrackHint)
        {
            double bestDist = std::numeric_limits<double>::max();

            for (const Cluster* c : passed)
            {
                const cv::Point2f center(static_cast<float>(c->fit.cx), static_cast<float>(c->fit.cy));
                const double d = Distance(center, gate.trackHint);

                if (d < bestDist)
                {
                    bestDist = d;
                    best = c;
                }
            }
        }
        else
        {
            std::size_t bestCount = 0;

            for (const Cluster* c : passed)
            {
                if (c->points.size() > bestCount)
                {
                    bestCount = c->points.size();
                    best = c;
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
        result.eventCentroid = best->centroid;
        result.fit = best->fit;
        result.radiusTrusted = best->fit.radiusTrusted;
        result.eventsUsed = static_cast<int>(best->points.size());
        result.aspectRatio = best->aspectRatio;
        result.fitRmsRatio = best->rmsRatio;
        result.boundingBox = best->boundingBox;
        result.reject = BallRejectReason::None;

        return result;
    }
}
