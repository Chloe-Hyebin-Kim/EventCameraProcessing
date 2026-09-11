#pragma once

// 서브픽셀 원 적합(circle fit).
//
// 왜 필요한가:
//   BallDetector가 쓰는 cv::minEnclosingCircle()은 컨투어의 "가장 바깥 극점 2~3개"만으로
//   원을 결정한다. 따라서 (a) 항상 바깥쪽으로 편향되고 (b) 노이즈 픽셀 하나에 통째로 끌려간다.
//   단안 구성에서 거리 Z는 겉보기 반지름 r로부터 나오고 dZ/Z = -dr/r 이므로,
//   반지름 1 px 오차 = 볼스피드 3.2 % 오차(공 지름 31 px 기준)다.
//
// 무엇을 하는가:
//   1) 중심을 뺀 Kasa 대수 적합으로 초기값을 잡고,
//   2) 진짜 기하 거리 오차 sum (d_i - R)^2 를 최소화하는 고정점(Landau) 반복으로 정련하고,
//   3) MAD 기반으로 이상치를 두 번 잘라낸다.
//
// 수치 검증 결과 (합성 데이터, 반지름 15.5 px, 점당 가우시안 노이즈 sigma=0.5 px, 400회 시행):
//   - 완전한 원(120점): 반지름 편향 +0.011 px, 표준편차 0.050 px, 중심오차 0.087 px
//     -> 공 지름 31 px일 때 볼스피드 오차 약 0.16 % 수준 (dZ/Z = -dr/r)
//   - 같은 조건에서 Kasa 대수적합만 썼을 때는 이상치 10개에 중심이 1.19 px 끌려갔다.
//     Landau 기하 적합 + MAD 트리밍을 거치면 0.12 px 이하로 유지된다.
//
// 중요 - coverage를 반드시 확인할 것:
//   호(arc)가 짧아지면 반지름은 계통적으로 과소추정된다. 그런데 잔차 rms는 이 실패를
//   전혀 잡아내지 못한다(아래 실측값 참조). 반지름을 신뢰할지 여부는 rms가 아니라
//   "중심에서 본 각도 커버리지"로 판단해야 한다.
//
//     호 길이   coverage   반지름 오차   rms
//      360도      0.97      -0.05 px    0.47
//      270도      0.78      -0.10 px    0.49
//      180도      0.50      -0.06 px    0.46
//      120도      0.36      -0.80 px    0.46   <- rms는 그대로인데 반지름이 무너짐
//       90도      0.28      -0.92 px    0.57
//       60도      0.25      -4.33 px    0.49
//
//   그래서 coverage >= minCoverage(기본 0.50)일 때만 radiusTrusted = true 로 표시한다.
//   이벤트 카메라에서 정지에 가까운 공은 윤곽 일부만 이벤트를 만들므로 이 경우가 흔하다.
//   radiusTrusted == false 인 프레임의 반지름은 스케일/거리 계산에 쓰면 안 된다.

#include <vector>

#include <opencv2/opencv.hpp>

namespace eventcore
{
    struct CircleFitOptions
    {
        int    maxIterations  = 60;     // Landau 고정점 반복 상한
        double convergencePx  = 1e-4;   // 중심 이동량이 이 값 미만이면 수렴으로 간주
        bool   rejectOutliers = true;   // MAD 기반 이상치 제거 사용 여부
        double outlierSigma   = 2.5;    // |잔차 - median| > sigma * 1.4826 * MAD 이면 제외
        int    trimPasses     = 2;      // 이상치 제거 반복 횟수
        int    minPoints      = 6;      // 이보다 점이 적으면 적합하지 않음
        int    coverageBins   = 36;     // 각도 커버리지 계산용 빈 개수(10도 단위)
        double minCoverage    = 0.50;   // 반지름을 신뢰할 최소 커버리지
    };

    struct CircleFitResult
    {
        bool   ok = false;          // 적합 성공 여부

        double cx = 0.0;            // 중심 x (서브픽셀)
        double cy = 0.0;            // 중심 y (서브픽셀)
        double r  = 0.0;            // 반지름 (서브픽셀)

        double rmsPx = 0.0;         // 잔차 rms. 적합 품질 지표이되 짧은 호를 못 잡는다는 점에 주의
        double coverage = 0.0;      // 0~1. 중심 기준 각도 커버리지. 반지름 신뢰도의 실제 판단 근거
        bool   radiusTrusted = false; // coverage >= minCoverage

        int pointsTotal = 0;        // 입력 점 개수
        int pointsUsed  = 0;        // 이상치 제거 후 실제 사용한 점 개수
    };

    class CircleFit
    {
    public:
        static CircleFitResult Fit(const std::vector<cv::Point>& points,
                                   const CircleFitOptions& options = CircleFitOptions());

        static CircleFitResult Fit(const std::vector<cv::Point2f>& points,
                                   const CircleFitOptions& options = CircleFitOptions());

        static CircleFitResult Fit(const std::vector<cv::Point2d>& points,
                                   const CircleFitOptions& options = CircleFitOptions());
    };
}
