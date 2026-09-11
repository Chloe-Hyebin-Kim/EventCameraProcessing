#pragma once

// 강건한 공 검출기.
//
// 기존 BallDetector::Detect()의 문제:
//   컨투어 중 "면적이 가장 큰 것"을 형상 검사 없이 무조건 고른다(area > 20 만 확인).
//   그런데 다운스윙 중에는 클럽 헤드와 샤프트가 공보다 훨씬 많은 이벤트를 만든다.
//   즉 정작 측정이 필요한 임팩트 순간에 최대 컨투어는 공이 아니라 클럽이다.
//
// 여기서 하는 일:
//   1) 모든 외곽 컨투어를 후보로 놓고 형상 게이트를 통과시킨다
//        - 면적 범위
//        - 원형도 circularity = 4*pi*A / P^2   (완전한 원 = 1.0, 가늘고 긴 샤프트 ~ 0)
//        - 바운딩박스 종횡비                    (클럽 샤프트는 여기서 걸러진다)
//        - 원 적합 후 반지름 범위
//        - 직전 프레임 위치 근처인지(트래킹 게이트)
//   2) 살아남은 후보 중 하나를 고른다
//        - 트래킹 힌트가 있으면 가장 가까운 것
//        - 없으면 면적이 가장 큰 것
//   3) 중심/반지름을 CircleFit(서브픽셀 기하 적합)으로 다시 구한다
//
// 반환값의 ToLegacy()는 기존 BallDetectionResult를 그대로 만들어 주므로
// ShotTrigger에 그대로 넣을 수 있다.

#include <vector>

#include <opencv2/opencv.hpp>

#include "BallDetector.h"
#include "CircleFit.h"

namespace eventcore
{
    struct BallGate
    {
        double minAreaPx = 20.0;        // 이보다 작은 컨투어는 노이즈로 간주
        double maxAreaPx = 0.0;         // 0 이면 상한 없음

        float minRadiusPx = 3.0f;       // 적합된 반지름 하한
        float maxRadiusPx = 0.0f;       // 0 이면 상한 없음.
                                        // 표준거리가 정해지면 반드시 설정할 것.
                                        // 예) 공 지름 31 px 예상 -> 약 10 ~ 28 px로 잡으면
                                        //     비행 중 생기는 긴 얼룩(반지름 수백 px)이 걸러진다

        double minCircularity = 0.55;   // 4*pi*A / P^2. 0.0 이면 검사 안 함
        double maxAspectRatio = 2.2;    // 바운딩박스 긴변/짧은변. 0.0 이면 검사 안 함

        bool useTrackHint = false;      // 직전 프레임 중심을 힌트로 사용
        cv::Point2f trackHint = cv::Point2f(0.0f, 0.0f);
        float trackGatePx = 0.0f;       // 힌트로부터 이 거리 안의 후보만 인정. 0 이면 거리 제한 없음
    };

    enum class BallRejectReason
    {
        None = 0,
        EmptyImage,
        NoContours,
        AllRejectedByGate,
        FitFailed,
    };

    struct RobustBallResult
    {
        bool detected = false;

        cv::Point2f center = cv::Point2f(0.0f, 0.0f);   // 서브픽셀 (CircleFit)
        float radius = 0.0f;                             // 서브픽셀 (CircleFit)
        cv::Point2f momentCentroid = cv::Point2f(0.0f, 0.0f); // 마스크 모멘트 무게중심(교차검증용)

        double area = 0.0;
        double perimeter = 0.0;
        double circularity = 0.0;
        double aspectRatio = 0.0;
        cv::Rect boundingBox;

        CircleFitResult fit;
        bool radiusTrusted = false;     // fit.radiusTrusted 와 동일. false면 스케일 계산에 쓰지 말 것

        int contoursTotal = 0;          // 이번 프레임의 전체 외곽 컨투어 수
        int contoursPassed = 0;         // 게이트를 통과한 후보 수
        BallRejectReason reject = BallRejectReason::None;

        // 기존 파이프라인(ShotTrigger 등)에 그대로 넘기기 위한 변환
        BallDetectionResult ToLegacy() const;
    };

    class RobustBallDetector
    {
    public:
        static RobustBallResult Detect(const cv::Mat& binaryImage,
                                       const BallGate& gate = BallGate(),
                                       const CircleFitOptions& fitOptions = CircleFitOptions());

        static const char* RejectReasonName(BallRejectReason reason);
    };
}
