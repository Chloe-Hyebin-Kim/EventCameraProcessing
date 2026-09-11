#pragma once

// 링버퍼 + 강건 검출기 + ShotTrigger + 측정 로거를 한 덩어리로 묶은 것.
// 호출 측(MainWindow, Console)은 윈도우마다 OnWindow()를 한 번 부르면 된다.
//
// 동작:
//   1) 들어온 raw 이벤트를 항상 링버퍼에 쌓는다(프리트리거 히스토리).
//   2) RobustBallDetector로 공을 찾는다(직전 위치를 트래킹 힌트로 사용).
//   3) ShotTrigger를 갱신한다.
//   4) 측정값 한 줄을 CSV에 남긴다.
//   5) 트리거가 걸리면 트리거 시각을 기억해 두고, 링버퍼에 postTriggerUs 만큼 더 쌓인 뒤
//      [trigger - preTriggerUs, trigger + postTriggerUs) 구간을 통째로 덤프한다.
//
// 5번이 핵심이다. 덤프는 트리거 시점 "이전"까지 되감아 저장되므로,
// 임팩트 직후 launch window가 온전히 파일에 남는다.
//
// 스레드:
//   단일 스레드에서만 호출할 것. LiveEventStream의 콜백은 워커 스레드에서 오므로,
//   UI 스레드로 마샬링한 뒤에 부르거나(현재 MainWindow가 이미 그렇게 한다),
//   호출 측에서 뮤텍스로 감쌀 것.

#include <string>
#include <vector>

#include <opencv2/opencv.hpp>

#include "Event.h"
#include "EventBallFinder.h"
#include "EventRingBuffer.h"
#include "RobustBallDetector.h"
#include "ShotLogger.h"
#include "ShotTrigger.h"

namespace eventcore
{
    struct ShotRecorderConfig
    {
        std::string outputDir = "output";   // 샷마다 이 아래에 shot_0001/ 식으로 폴더가 생긴다

        EventRingBufferConfig ring;         // 기본 300 ms / 1200만 이벤트
        ShotTriggerConfig trigger;
        BallGate gate;
        CircleFitOptions fit;

        // 검출 경로 선택.
        //   true  (기본) : EventBallFinder - raw 이벤트 점군에 직접 원 적합
        //   false        : RobustBallDetector - 이진 마스크 컨투어 기반
        //
        // 짧은 윈도우(스핀 측정에 필요한 수백 us)에서는 공이 성긴 점군이 되어
        // 마스크/컨투어 경로가 파편화된다. 실측에서 5 ms 윈도우에 윤곽 이벤트 40개 수준이면
        // 컨투어가 24개로 쪼개져 검출률이 0 %였다. 그래서 점군 경로가 기본값이다.
        // 긴 윈도우(10 ms 이상)에서 기존 동작과 비교하고 싶을 때만 false로 둘 것.
        bool useEventCloud = true;
        EventBallFinderConfig cloud;

        lli preTriggerUs = 30000;           // 트리거 이전 30 ms
        lli postTriggerUs = 30000;          // 트리거 이후 30 ms
                                            // 드라이버 70 m/s 기준 30 ms = 2.1 m 이동.
                                            // launch window(30 cm = 4.3 ms)를 충분히 덮는다.

        bool dumpEvents = true;             // 트리거 시 raw 이벤트 CSV 덤프
        bool logMeasurements = true;        // 윈도우별 측정 CSV

        // 이탈(departure) 트리거.
        //
        // 왜 필요한가 (합성 데이터로 확인한 실패):
        //   비행 중인 공은 어떤 형상 게이트도 통과하지 못한다. 5 ms 윈도우에서 70 m/s 급으로
        //   움직이면 클러스터가 수백 px 길이의 줄무늬가 되어 종횡비/반지름 게이트에 걸린다.
        //   즉 "움직이는 공의 중심 이동속도"로 샷을 판정하는 ShotTrigger 방식은
        //   원리적으로 성립하지 않는다. 실제로 위 조건에서 트리거가 한 번도 걸리지 않았다.
        //
        // 대신 두 가지를 본다:
        //   (a) 어드레스 위치에 있던 공이 사라졌다
        //   (b) 그 순간 전역 이벤트 수가 최근 기준선의 burstFactor 배 이상으로 튀었다
        //       (클럽 진입 + 공 이탈은 반드시 큰 이벤트 폭증을 만든다)
        //
        // 이 조합은 공을 "추적"할 필요가 없으므로 비행 중 검출 실패와 무관하게 동작한다.
        bool useDepartureTrigger = true;
        int baselineWindows = 32;           // 기준선 계산에 쓸 최근 윈도우 수
        double burstFactor = 3.0;           // 기준선의 이 배수를 넘으면 폭증으로 간주
        int minBurstEvents = 50;            // 폭증 판정의 절대 하한(조용한 장면 보호)
        float departurePx = 25.0f;          // 어드레스 위치에서 이만큼 벗어나면 이탈로 간주
        lli refractoryUs = 500000;          // 트리거 후 이 시간 동안은 다시 트리거하지 않는다.
                                            // 트리거 직후에는 ShotTrigger를 리셋해 새 공이
                                            // 다시 Ready가 될 때까지 기다리게 하지만,
                                            // 불응기가 있어야 한 샷이 여러 번 잡히지 않는다.

        // 트리거 확인(confirm).
        //
        // 노이즈 클러스터가 한 윈도우 튀면 중심 이동속도가 순간적으로 치솟아 ShotTrigger가
        // 헛트리거를 낸다(합성 데이터에서 실제로 재현됨). 진짜 샷이라면 공은 원래 자리로
        // 돌아오지 않는다. 그래서 트리거 이후 confirmUs 동안 공이 트리거 직전의 정지 위치
        // 근처에서 다시 발견되면 그 트리거를 취소한다.
        //
        // 확인 때문에 덤프가 늦어지는 것은 비용이 아니다. 링버퍼가 트리거 시점 이전을
        // 되감아 저장하므로, 늦게 확정해도 저장되는 구간은 똑같다.
        bool triggerConfirm = true;
        float confirmReturnPx = 20.0f;      // 이 거리 안에서 공이 다시 보이면 헛트리거로 판단

        // 트래킹 힌트를 쓸지. 켜면 직전 프레임 중심 근처의 후보를 우선한다.
        bool useTracking = true;
        float trackGatePx = 120.0f;         // 힌트로부터 이 거리 안의 후보만 인정
        lli trackTimeoutUs = 200000;        // 이 시간 이상 미검출이면 힌트를 버린다
    };

    struct ShotRecorderUpdate
    {
        // useEventCloud == true 이면 cloudBall이, false이면 maskBall이 채워진다.
        // ball 은 어느 경로든 공통으로 채워지는 요약값이다.
        BallDetectionResult ball;
        bool detected = false;
        float radius = 0.0f;
        cv::Point2f center = cv::Point2f(0.0f, 0.0f);
        bool radiusTrusted = false;
        CircleFitResult fit;
        BallRejectReason reject = BallRejectReason::None;

        EventBallResult cloudBall;
        RobustBallResult maskBall;
        ShotUpdateResult shot;

        bool dumpWritten = false;           // 이번 호출에서 덤프 파일이 만들어졌는지
        std::string dumpPath;               // 덤프한 이벤트 CSV 경로
        long long dumpEventCount = 0;
        lli dumpFromUs = 0;
        lli dumpToUs = 0;

        bool ringOverflowed = false;        // 메모리 상한 때문에 이벤트가 버려졌는지 -> maxEvents 부족
        bool triggerCancelled = false;      // 확인 단계에서 헛트리거로 판정되어 취소됨
    };

    class ShotRecorder
    {
    public:
        ShotRecorder();
        ~ShotRecorder();

        ShotRecorder(const ShotRecorder&) = delete;
        ShotRecorder& operator=(const ShotRecorder&) = delete;

        // 스트림을 시작하기 전에 한 번 호출한다. 출력 폴더를 만들고 측정 CSV를 연다.
        bool Begin(const ShotRecorderConfig& config);

        // 윈도우마다 한 번 호출한다.
        //   windowEvents : 이 윈도우의 raw 이벤트(링버퍼에 쌓인다)
        //   binaryMask   : EventProcessor::Process() 결과의 binaryMask
        ShotRecorderUpdate OnWindow(const std::vector<Event>& windowEvents,
                                    const cv::Mat& binaryMask,
                                    lli windowStartUs,
                                    lli windowEndUs);

        // 이벤트 점군 경로는 센서 크기를 알아야 한다. binaryMask가 비어 있을 수 있는
        // 경로(마스크를 아예 만들지 않는 경우)에서는 이걸 먼저 불러 둘 것.
        void SetSensorSize(int width, int height) { m_width = width; m_height = height; }

        // 스트림 종료 시 호출. 아직 덤프되지 않은 대기 중인 샷이 있으면 가진 만큼이라도 저장한다.
        void End();

        ShotState State() const { return m_trigger.State(); }
        const EventRingBuffer& Ring() const { return m_ring; }
        int ShotCount() const { return m_shotIndex; }
        const std::string& MeasurementCsvPath() const { return m_logger.Path(); }

    private:
        bool FlushPendingDump(ShotRecorderUpdate& update, bool force);

        ShotRecorderConfig m_config;
        EventRingBuffer m_ring;
        ShotTrigger m_trigger;
        ShotLogger m_logger;

        bool m_active = false;
        int m_shotIndex = 0;

        int m_width = WIDTH;
        int m_height = HEIGHT;

        bool m_dumpPending = false;
        lli m_pendingTriggerUs = 0;
        bool m_pendingCancelled = false;
        cv::Point2f m_pendingAnchor = cv::Point2f(0.0f, 0.0f);
        bool m_havePendingAnchor = false;

        bool m_haveStableCenter = false;
        cv::Point2f m_stableCenter = cv::Point2f(0.0f, 0.0f);

        std::vector<int> m_baseline;        // 최근 윈도우별 이벤트 수 (기준선용)
        std::size_t m_baselineNext = 0;
        lli m_lastTriggerUs = -1;

        bool m_haveTrack = false;
        cv::Point2f m_trackHint = cv::Point2f(0.0f, 0.0f);
        lli m_trackUs = 0;

        bool m_havePrevCenter = false;
        cv::Point2f m_prevCenter = cv::Point2f(0.0f, 0.0f);
        lli m_prevCenterUs = 0;
    };
}
