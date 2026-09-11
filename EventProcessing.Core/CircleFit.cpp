#include "pch.h"
#include "CircleFit.h"

#include <algorithm>
#include <cmath>
#include <vector>

namespace eventcore
{
    namespace
    {
        double Median(std::vector<double>& v)
        {
            if (v.empty())
            {
                return 0.0;
            }

            const size_t mid = v.size() / 2;
            std::nth_element(v.begin(), v.begin() + mid, v.end());
            const double hi = v[mid];

            if (v.size() % 2 != 0)
            {
                return hi;
            }

            // 짝수 개일 때는 중앙 두 값의 평균. nth_element는 mid 왼쪽이 모두 hi 이하임을 보장한다.
            const double lo = *std::max_element(v.begin(), v.begin() + mid);
            return 0.5 * (lo + hi);
        }

        // 중심을 뺀 Kasa 대수 적합. 초기값 전용이며, 짧은 호에서는 편향이 크다.
        bool Kasa(const std::vector<cv::Point2d>& p, double& cx, double& cy, double& r)
        {
            const size_t n = p.size();

            double mx = 0.0;
            double my = 0.0;

            for (const cv::Point2d& q : p)
            {
                mx += q.x;
                my += q.y;
            }

            mx /= static_cast<double>(n);
            my /= static_cast<double>(n);

            double Suu = 0.0, Svv = 0.0, Suv = 0.0;
            double Suuu = 0.0, Svvv = 0.0, Suvv = 0.0, Svuu = 0.0;

            for (const cv::Point2d& q : p)
            {
                const double u = q.x - mx;
                const double v = q.y - my;
                const double uu = u * u;
                const double vv = v * v;

                Suu += uu;
                Svv += vv;
                Suv += u * v;
                Suuu += uu * u;
                Svvv += vv * v;
                Suvv += u * vv;
                Svuu += v * uu;
            }

            const double det = Suu * Svv - Suv * Suv;

            if (std::abs(det) < 1e-12)
            {
                // 점들이 거의 한 직선 위에 있음 -> 원을 정의할 수 없다.
                return false;
            }

            const double b0 = 0.5 * (Suuu + Suvv);
            const double b1 = 0.5 * (Svvv + Svuu);

            const double uc = (b0 * Svv - b1 * Suv) / det;
            const double vc = (Suu * b1 - Suv * b0) / det;

            cx = uc + mx;
            cy = vc + my;

            double acc = 0.0;

            for (const cv::Point2d& q : p)
            {
                const double dx = q.x - cx;
                const double dy = q.y - cy;
                acc += dx * dx + dy * dy;
            }

            r = std::sqrt(acc / static_cast<double>(n));

            return std::isfinite(cx) && std::isfinite(cy) && std::isfinite(r);
        }

        // sum (d_i - R)^2 를 최소화하는 Landau 고정점 반복.
        //   dF/da = 0  =>  a = mean(x) - R * mean((x_i - a) / d_i)
        // R은 매 반복에서 mean(d_i)로 갱신된다.
        void Landau(const std::vector<cv::Point2d>& p, double& cx, double& cy, double& r,
                    int maxIterations, double convergencePx)
        {
            const size_t n = p.size();

            double mx = 0.0;
            double my = 0.0;

            for (const cv::Point2d& q : p)
            {
                mx += q.x;
                my += q.y;
            }

            mx /= static_cast<double>(n);
            my /= static_cast<double>(n);

            for (int iter = 0; iter < maxIterations; ++iter)
            {
                double sumD = 0.0;
                double sumUx = 0.0;
                double sumUy = 0.0;

                for (const cv::Point2d& q : p)
                {
                    const double dx = q.x - cx;
                    const double dy = q.y - cy;
                    double d = std::sqrt(dx * dx + dy * dy);

                    if (d < 1e-12)
                    {
                        d = 1e-12;
                    }

                    sumD += d;
                    sumUx += dx / d;
                    sumUy += dy / d;
                }

                const double R = sumD / static_cast<double>(n);
                const double nextX = mx - R * (sumUx / static_cast<double>(n));
                const double nextY = my - R * (sumUy / static_cast<double>(n));

                if (!std::isfinite(nextX) || !std::isfinite(nextY))
                {
                    break;
                }

                const double step = std::max(std::abs(nextX - cx), std::abs(nextY - cy));

                cx = nextX;
                cy = nextY;

                if (step < convergencePx)
                {
                    break;
                }
            }

            double acc = 0.0;

            for (const cv::Point2d& q : p)
            {
                const double dx = q.x - cx;
                const double dy = q.y - cy;
                acc += std::sqrt(dx * dx + dy * dy);
            }

            r = acc / static_cast<double>(n);
        }

        double Coverage(const std::vector<cv::Point2d>& p, double cx, double cy, int bins)
        {
            if (bins <= 0)
            {
                return 0.0;
            }

            std::vector<char> hit(static_cast<size_t>(bins), 0);

            for (const cv::Point2d& q : p)
            {
                const double theta = std::atan2(q.y - cy, q.x - cx);   // -pi .. pi
                int idx = static_cast<int>((theta + CV_PI) / (2.0 * CV_PI) * bins);

                if (idx < 0)
                {
                    idx = 0;
                }

                if (idx >= bins)
                {
                    idx = bins - 1;
                }

                hit[static_cast<size_t>(idx)] = 1;
            }

            int occupied = 0;

            for (char h : hit)
            {
                occupied += (h != 0) ? 1 : 0;
            }

            return static_cast<double>(occupied) / static_cast<double>(bins);
        }

        double RmsResidual(const std::vector<cv::Point2d>& p, double cx, double cy, double r)
        {
            if (p.empty())
            {
                return 0.0;
            }

            double acc = 0.0;

            for (const cv::Point2d& q : p)
            {
                const double dx = q.x - cx;
                const double dy = q.y - cy;
                const double e = std::sqrt(dx * dx + dy * dy) - r;
                acc += e * e;
            }

            return std::sqrt(acc / static_cast<double>(p.size()));
        }
    }

    CircleFitResult CircleFit::Fit(const std::vector<cv::Point2d>& points, const CircleFitOptions& options)
    {
        CircleFitResult result;
        result.pointsTotal = static_cast<int>(points.size());

        if (result.pointsTotal < options.minPoints)
        {
            return result;
        }

        double cx = 0.0;
        double cy = 0.0;
        double r = 0.0;

        if (!Kasa(points, cx, cy, r))
        {
            return result;
        }

        Landau(points, cx, cy, r, options.maxIterations, options.convergencePx);

        std::vector<cv::Point2d> used = points;

        if (options.rejectOutliers && result.pointsTotal >= 8)
        {
            for (int pass = 0; pass < options.trimPasses; ++pass)
            {
                std::vector<double> residual;
                residual.reserve(used.size());

                for (const cv::Point2d& q : used)
                {
                    const double dx = q.x - cx;
                    const double dy = q.y - cy;
                    residual.push_back(std::sqrt(dx * dx + dy * dy) - r);
                }

                std::vector<double> tmp = residual;
                const double med = Median(tmp);

                std::vector<double> absDev;
                absDev.reserve(residual.size());

                for (double e : residual)
                {
                    absDev.push_back(std::abs(e - med));
                }

                const double mad = Median(absDev);

                if (mad < 1e-9)
                {
                    break;  // 잔차가 사실상 0 -> 더 잘라낼 것이 없다
                }

                const double limit = options.outlierSigma * 1.4826 * mad;

                std::vector<cv::Point2d> keep;
                keep.reserve(used.size());

                for (size_t i = 0; i < used.size(); ++i)
                {
                    if (std::abs(residual[i] - med) <= limit)
                    {
                        keep.push_back(used[i]);
                    }
                }

                // 너무 많이 잘라내면(원본의 40 % 또는 6점 미만) 적합이 오히려 불안정해진다.
                const size_t floorCount = std::max<size_t>(6, static_cast<size_t>(0.4 * points.size()));

                if (keep.size() < floorCount || keep.size() == used.size())
                {
                    break;
                }

                used.swap(keep);
                Landau(used, cx, cy, r, options.maxIterations, options.convergencePx);
            }
        }

        result.ok = std::isfinite(cx) && std::isfinite(cy) && std::isfinite(r) && r > 0.0;

        if (!result.ok)
        {
            return result;
        }

        result.cx = cx;
        result.cy = cy;
        result.r = r;
        result.pointsUsed = static_cast<int>(used.size());
        result.rmsPx = RmsResidual(used, cx, cy, r);

        // coverage는 이상치를 제거하기 전 전체 점으로 계산한다.
        // 실제로 윤곽이 어느 각도 범위에 걸쳐 있는지가 알고 싶은 것이기 때문이다.
        result.coverage = Coverage(points, cx, cy, options.coverageBins);
        result.radiusTrusted = (result.coverage >= options.minCoverage);

        return result;
    }

    CircleFitResult CircleFit::Fit(const std::vector<cv::Point>& points, const CircleFitOptions& options)
    {
        std::vector<cv::Point2d> converted;
        converted.reserve(points.size());

        for (const cv::Point& p : points)
        {
            converted.emplace_back(static_cast<double>(p.x), static_cast<double>(p.y));
        }

        return Fit(converted, options);
    }

    CircleFitResult CircleFit::Fit(const std::vector<cv::Point2f>& points, const CircleFitOptions& options)
    {
        std::vector<cv::Point2d> converted;
        converted.reserve(points.size());

        for (const cv::Point2f& p : points)
        {
            converted.emplace_back(static_cast<double>(p.x), static_cast<double>(p.y));
        }

        return Fit(converted, options);
    }
}
