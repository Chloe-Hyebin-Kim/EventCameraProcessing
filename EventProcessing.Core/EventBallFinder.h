#pragma once

// 이벤트 점군에서 직접 공을 찾는다. 이진 마스크의 컨투어를 쓰지 않는다.
//
// 왜 필요한가 (실측으로 확인한 문제):
//   윈도우를 짧게 잡으면 - 스핀 측정에는 반드시 그래야 한다 - 공은 "연결된 덩어리"가 아니라
//   "성긴 점군"이 된다. 실제로 5 ms 윈도우에 공 윤곽 이벤트 40개 수준으로 합성 데이터를 만들어
//   기존 경로(EventAccumulator -> 정규화 -> Otsu -> MORPH_OPEN/CLOSE -> findContours)에 넣으면
//   컨투어가 24개의 파편으로 쪼개지고, 어떤 것도 공으로 인정되지 않는다. 검출률 0 %였다.
//
//   즉 마스크 + 컨투어는 긴 윈도우에서만 성립하는 방식이다.
//   200 us 윈도우로 내려가면 (로드맵 04절: 웨지 10,000 rpm에서 12도) 반드시 깨진다.
//
// 무엇을 하는가:
//   1) 이벤트를 cellPx 크기의 성긴 격자에 세어 밀도맵을 만든다
//   2) 점유 셀들을 연결성분으로 묶어 후보 클러스터를 만든다 (형태학 연산 없음)
//   3) 각 클러스터의 raw 이벤트 좌표에 CircleFit을 직접 돌린다
//   4) 반지름 범위 / 각도 커버리지 / 적합 잔차비로 게이트한다
//
//   컨투어 픽셀이 아니라 이벤트 좌표 전부를 쓰므로 적합에 들어가는 표본이 더 많고,
//   성김에 강하며, 형태학 연산이 신호를 지울 위험이 없다.
//
// 공의 이벤트는 실루엣 가장자리에 링 모양으로 생긴다. 그래서 원 적합이 자연스러운 원시연산이다.
// 클럽 샤프트처럼 길게 늘어진 것은 작은 반지름의 원으로 적합되지 않으므로
// maxRadiusPx와 잔차비 게이트에서 걸린다.

#include <vector>

#include <opencv2/opencv.hpp>

#include "BallDetector.h"
#include "CircleFit.h"
#include "Event.h"
#include "RobustBallDetector.h"   // BallGate, BallRejectReason 재사용

namespace eventcore
{
    struct EventBallFinderConfig
    {
        int cellPx = 8;                 // 밀도 격자 셀 크기(px)
        int minEventsPerCell = 1;       // 이 개수 이상이면 점유 셀
        int minCellsPerCluster = 3;     // 클러스터로 인정할 최소 셀 수
        int minEventsPerCluster = 12;   // 클러스터로 인정할 최소 이벤트 수.
                                        // 이보다 적으면 원 적합 자체가 신뢰할 수 없다
                                        // (검증: 완전한 원 120점에서 반지름 표준편차 0.05 px,
                                        //  점이 줄수록 급격히 나빠진다)
        // 셀 연결 반경(체비쇼프 거리, 셀 단위).
        //   1 = 8-이웃만 연결
        //   2 = 한 칸 건너뛴 셀까지 연결 (기본)
        // 성긴 링은 셀이 띄엄띄엄 점유되어 8-이웃만으로는 조각난다.
        // 실측: 반지름 15 px 링에 이벤트 15개일 때 반경 1로는 검출 실패, 반경 2로는 성공.
        int linkRadiusCells = 2;

        // 적합 잔차 / 반지름. 링 모양이면 작고, 덩어리나 직선이면 커진다.
        double maxFitRmsRatio = 0.25;   // 0이면 검사 안 함
    };

    struct EventBallResult
    {
        bool detected = false;

        cv::Point2f center = cv::Point2f(0.0f, 0.0f);   // 서브픽셀
        float radius = 0.0f;                             // 서브픽셀
        cv::Point2f eventCentroid = cv::Point2f(0.0f, 0.0f);

        CircleFitResult fit;
        bool radiusTrusted = false;

        int eventsUsed = 0;             // 적합에 쓴 이벤트 수
        double aspectRatio = 0.0;       // 클러스터 바운딩박스 긴변/짧은변
        double fitRmsRatio = 0.0;       // fit.rmsPx / fit.r
        cv::Rect boundingBox;

        int clustersTotal = 0;
        int clustersPassed = 0;
        BallRejectReason reject = BallRejectReason::None;

        BallDetectionResult ToLegacy() const;
    };

    class EventBallFinder
    {
    public:
        static EventBallResult Find(const std::vector<Event>& events,
                                    int width,
                                    int height,
                                    const BallGate& gate = BallGate(),
                                    const EventBallFinderConfig& config = EventBallFinderConfig(),
                                    const CircleFitOptions& fitOptions = CircleFitOptions());
    };
}
