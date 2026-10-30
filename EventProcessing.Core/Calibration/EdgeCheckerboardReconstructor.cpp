// Calibration/ 하위 폴더 - MSVC PCH 미사용(.vcxproj: PrecompiledHeader=NotUsing).
#include "EdgeCheckerboardReconstructor.h"

#include <algorithm>
#include <cmath>
#include <cstdint>
#include <numeric>
#include <unordered_map>
#include <vector>

namespace eventcore
{
    namespace
    {
        constexpr int kDensityWindow = 5;          // 활동 밀도를 재는 창(픽셀)
        constexpr int kMinCellAreaPx = 60;         // 이보다 작은 영역은 칸으로 보지 않음(노이즈 구멍)
        constexpr double kMinCellFill = 0.5;       // 영역 면적 / bounding box 면적 하한(가늘고 긴 조각 제외)
        constexpr double kCellAreaLow = 0.3;       // 칸 면적 허용 범위: 중앙값의 0.3배 ~ 3배
        constexpr double kCellAreaHigh = 3.0;
        constexpr double kMaxBandPerSide = 0.9;    // 이보다 두꺼운 경계 띠 너머 영역은 이웃으로 보지 않음(칸 한 변 대비)
        constexpr double kMinContactPerSide = 0.35;  // 변 이웃으로 인정할 공유 경계 길이(칸 한 변 대비)

        // 패리티 union-find: parity[x] = x와 부모의 색 차이(0 같음, 1 다름).
        struct ParityUnionFind
        {
            std::vector<int> parent;
            std::vector<int> parity;

            explicit ParityUnionFind(int n) : parent(n), parity(n, 0)
            {
                std::iota(parent.begin(), parent.end(), 0);
            }

            // 루트와 (x 기준) 루트와의 색 차이를 반환. 경로 압축 포함.
            std::pair<int, int> Find(int x)
            {
                int root = x;
                int acc = 0;
                while (parent[root] != root)
                {
                    acc ^= parity[root];
                    root = parent[root];
                }
                // 경로 압축: 각 노드의 parity를 루트 기준으로 갱신
                int cur = x;
                int curAcc = acc;
                while (parent[cur] != cur)
                {
                    const int next = parent[cur];
                    const int nextAcc = curAcc ^ parity[cur];
                    parent[cur] = root;
                    parity[cur] = curAcc;
                    cur = next;
                    curAcc = nextAcc;
                }
                return { root, acc };
            }
        };

        // 픽셀별 event 유무의 국소 밀도를 노이즈 밀도 기준으로 이진화해 경계 띠 마스크(255)를 만든다.
        cv::Mat BuildActivityMask(const cv::Mat& eventCount)
        {
            cv::Mat raw;
            cv::compare(eventCount, 0, raw, cv::CMP_GT);  // 0/255, CV_8U

            // 창 안의 활성 픽셀 개수(0..25). 정수라 중앙값을 히스토그램으로 정확히 구할 수 있다.
            cv::Mat count;
            cv::boxFilter(raw / 255, count, CV_32S, cv::Size(kDensityWindow, kDensityWindow), cv::Point(-1, -1),
                          /*normalize=*/false, cv::BORDER_REPLICATE);

            constexpr int kWindowArea = kDensityWindow * kDensityWindow;
            std::vector<long long> hist(kWindowArea + 1, 0);
            for (int y = 0; y < count.rows; ++y)
            {
                const int* row = count.ptr<int>(y);
                for (int x = 0; x < count.cols; ++x)
                {
                    ++hist[std::clamp(row[x], 0, kWindowArea)];
                }
            }

            // 대부분의 픽셀은 배경/칸 내부이므로 중앙값 = 노이즈 밀도 추정치.
            const long long half = static_cast<long long>(count.total()) / 2;
            long long acc = 0;
            int median = 0;
            for (int v = 0; v <= kWindowArea; ++v)
            {
                acc += hist[v];
                if (acc > half)
                {
                    median = v;
                    break;
                }
            }

            // 임계 = 노이즈 밀도와 1(완전 활성)의 중간.
            const double threshold = median + (kWindowArea - median) * 0.5;
            cv::Mat mask;
            cv::compare(count, threshold, mask, cv::CMP_GE);

            cv::morphologyEx(mask, mask, cv::MORPH_CLOSE,
                             cv::getStructuringElement(cv::MORPH_ELLIPSE, cv::Size(5, 5)));
            return mask;
        }
    }

    EdgeReconstruction EdgeCheckerboardReconstructor::Reconstruct(const cv::Mat& eventCount)
    {
        EdgeReconstruction result;
        if (eventCount.empty() || eventCount.channels() != 1)
        {
            return result;
        }

        const int width = eventCount.cols;
        const int height = eventCount.rows;

        const cv::Mat active = BuildActivityMask(eventCount);

        // 1) 경계가 아닌 영역 분할
        cv::Mat freeMask;
        cv::bitwise_not(active, freeMask);
        cv::Mat labels, stats, centroids;
        const int n = cv::connectedComponentsWithStats(freeMask, labels, stats, centroids, 4, CV_32S);
        if (n <= 1)
        {
            return result;
        }

        // 2) 칸 후보: 이미지 테두리에 닿지 않고, 너무 작지 않고, 꽉 찬 모양
        std::vector<int> candidates;
        for (int l = 1; l < n; ++l)
        {
            const int x = stats.at<int>(l, cv::CC_STAT_LEFT);
            const int y = stats.at<int>(l, cv::CC_STAT_TOP);
            const int w = stats.at<int>(l, cv::CC_STAT_WIDTH);
            const int h = stats.at<int>(l, cv::CC_STAT_HEIGHT);
            const int area = stats.at<int>(l, cv::CC_STAT_AREA);
            const bool touchesBorder = x == 0 || y == 0 || x + w >= width || y + h >= height;
            if (touchesBorder || area < kMinCellAreaPx)
            {
                continue;
            }
            if (static_cast<double>(area) / (static_cast<double>(w) * h) < kMinCellFill)
            {
                continue;
            }
            candidates.push_back(l);
        }
        if (candidates.size() < 4)
        {
            return result;
        }

        std::vector<int> candidateAreas;
        candidateAreas.reserve(candidates.size());
        for (int l : candidates)
        {
            candidateAreas.push_back(stats.at<int>(l, cv::CC_STAT_AREA));
        }
        std::nth_element(candidateAreas.begin(), candidateAreas.begin() + candidateAreas.size() / 2, candidateAreas.end());
        const double medianArea = candidateAreas[candidateAreas.size() / 2];
        const double side = std::sqrt(medianArea);  // 대표 칸 한 변 길이(px)

        std::vector<char> isCell(n, 0);
        for (int l : candidates)
        {
            const int area = stats.at<int>(l, cv::CC_STAT_AREA);
            if (area >= kCellAreaLow * medianArea && area <= kCellAreaHigh * medianArea)
            {
                isCell[l] = 1;
            }
        }

        // 3) Voronoi: 경계 띠 픽셀을 가장 가까운 비경계 픽셀의 영역 라벨에 배정한다.
        //    DIST_LABEL_PIXEL은 0 픽셀(=비경계)마다 고유 라벨을 주므로, 그 라벨 -> 영역 라벨 표를 직접 만든다.
        cv::Mat dist, voronoi;
        cv::distanceTransform(active, dist, voronoi, cv::DIST_L2, 5, cv::DIST_LABEL_PIXEL);

        double maxVoronoi = 0.0;
        cv::minMaxLoc(voronoi, nullptr, &maxVoronoi);
        std::vector<int> pixelLabelToRegion(static_cast<size_t>(maxVoronoi) + 1, 0);
        for (int y = 0; y < height; ++y)
        {
            const uchar* act = active.ptr<uchar>(y);
            const int* vor = voronoi.ptr<int>(y);
            const int* lab = labels.ptr<int>(y);
            for (int x = 0; x < width; ++x)
            {
                if (act[x] == 0)
                {
                    pixelLabelToRegion[vor[x]] = lab[x];
                }
            }
        }

        const float maxBand = static_cast<float>(std::max(4.0, kMaxBandPerSide * side));
        cv::Mat owner(height, width, CV_32SC1);
        for (int y = 0; y < height; ++y)
        {
            const uchar* act = active.ptr<uchar>(y);
            const int* vor = voronoi.ptr<int>(y);
            const int* lab = labels.ptr<int>(y);
            const float* d = dist.ptr<float>(y);
            int* own = owner.ptr<int>(y);
            for (int x = 0; x < width; ++x)
            {
                if (act[x] == 0)
                {
                    own[x] = lab[x];
                }
                else
                {
                    own[x] = d[x] <= maxBand ? pixelLabelToRegion[vor[x]] : 0;
                }
            }
        }

        // 영역 쌍별 공유 경계 길이(오른쪽/아래 이웃과 소유 영역이 다른 픽셀 쌍 수)
        std::unordered_map<std::int64_t, int> contact;
        const auto addContact = [&](int a, int b)
        {
            if (a == b || a <= 0 || b <= 0 || !(isCell[a] || isCell[b]))
            {
                return;
            }
            const std::int64_t lo = std::min(a, b);
            const std::int64_t hi = std::max(a, b);
            ++contact[lo * n + hi];
        };
        for (int y = 0; y < height; ++y)
        {
            const int* own = owner.ptr<int>(y);
            const int* below = y + 1 < height ? owner.ptr<int>(y + 1) : nullptr;
            for (int x = 0; x < width; ++x)
            {
                if (x + 1 < width)
                {
                    addContact(own[x], own[x + 1]);
                }
                if (below)
                {
                    addContact(own[x], below[x]);
                }
            }
        }

        // 4) 변 이웃(공유 경계가 긴) 순서로 "다른 색" 제약을 union-find에 넣는다.
        struct Contact
        {
            int length;
            int a;
            int b;
        };
        std::vector<Contact> edges;
        const double minContact = kMinContactPerSide * side;
        for (const auto& kv : contact)
        {
            if (kv.second >= minContact)
            {
                edges.push_back({ kv.second, static_cast<int>(kv.first / n), static_cast<int>(kv.first % n) });
            }
        }
        std::sort(edges.begin(), edges.end(), [](const Contact& l, const Contact& r) { return l.length > r.length; });

        ParityUnionFind uf(n);
        std::vector<char> inGraph(n, 0);
        int conflicts = 0;
        for (const Contact& e : edges)
        {
            inGraph[e.a] = inGraph[e.b] = 1;
            const auto [ra, pa] = uf.Find(e.a);
            const auto [rb, pb] = uf.Find(e.b);
            if (ra != rb)
            {
                uf.parent[rb] = ra;
                uf.parity[rb] = pa ^ pb ^ 1;
            }
            else if (pa == pb)
            {
                ++conflicts;  // 이미 같은 색으로 묶였는데 변 이웃 -> 2색 위반
            }
        }

        // 칸을 가장 많이 포함한 연결 그룹만 사용한다(떨어진 노이즈 그룹 제외).
        std::vector<int> cellsPerRoot(n, 0);
        for (int l = 1; l < n; ++l)
        {
            if (inGraph[l] && isCell[l])
            {
                ++cellsPerRoot[uf.Find(l).first];
            }
        }
        const int mainRoot = static_cast<int>(std::max_element(cellsPerRoot.begin(), cellsPerRoot.end()) - cellsPerRoot.begin());
        const int mainCells = cellsPerRoot[mainRoot];
        if (mainCells < 4)
        {
            return result;
        }

        std::vector<int> colorOf(n, -1);
        for (int l = 1; l < n; ++l)
        {
            if (inGraph[l])
            {
                const auto [root, parity] = uf.Find(l);
                if (root == mainRoot)
                {
                    colorOf[l] = parity;
                }
            }
        }

        // 5) 렌더링. 칸이 아닌 큰 영역(여백/배경)은 색이 정해진 칸 근처로만 칠한다.
        cv::Mat cellMask(height, width, CV_8UC1, cv::Scalar(0));
        for (int y = 0; y < height; ++y)
        {
            const int* lab = labels.ptr<int>(y);
            uchar* cm = cellMask.ptr<uchar>(y);
            for (int x = 0; x < width; ++x)
            {
                cm[x] = (isCell[lab[x]] && colorOf[lab[x]] >= 0) ? 255 : 0;
            }
        }
        const int nearSize = static_cast<int>(2.0 * side) | 1;
        cv::Mat nearCells;
        cv::dilate(cellMask, nearCells, cv::getStructuringElement(cv::MORPH_ELLIPSE, cv::Size(nearSize, nearSize)));

        result.image.create(height, width, CV_8UC1);
        for (int y = 0; y < height; ++y)
        {
            const int* own = owner.ptr<int>(y);
            const uchar* nearRow = nearCells.ptr<uchar>(y);
            uchar* dst = result.image.ptr<uchar>(y);
            for (int x = 0; x < width; ++x)
            {
                const int o = own[x];
                const int c = o > 0 ? colorOf[o] : -1;
                if (c < 0 || (!isCell[o] && nearRow[x] == 0))
                {
                    dst[x] = 127;
                }
                else
                {
                    dst[x] = c == 0 ? 255 : 0;
                }
            }
        }

        result.ok = true;
        result.cellCount = mainCells;
        result.conflictCount = conflicts;
        return result;
    }
}
