#pragma once

#include "BallDetector.h"
#include "Event.h"

namespace eventcore
{
    enum class ShotState
    {
        Searching,  // 공이 없거나, 있어도 아직 정지 상태가 N초 이상 확인되지 않음
        Ready,      // 같은 위치(stableMovePx 이내)에서 readySeconds 이상 유지됨 -> 레디 신호
        Capturing,  // Ready 상태에서 샷으로 판단된 시점부터 captureSeconds 동안 유지
    };

    struct ShotUpdateResult
    {
        ShotState state = ShotState::Searching;
        bool justEnteredReady = false;   // 이번 Update에서 처음 Ready로 전이함
        bool justTriggered = false;      // 이번 Update에서 샷이 트리거되어 Capturing 시작
        bool justFinishedCapture = false;// 이번 Update에서 captureSeconds가 끝나 Searching으로 복귀
    };

    struct ShotTriggerConfig
    {
        double readySeconds = 0.4;          // N1: 레디로 인정하기까지 정지 유지 시간(초)
        double captureSeconds = 2.5;        // N3: 트리거 이후 촬영 유지 시간(초)
        float stableMovePx = 6.0f;          // 레디 판정 중 허용하는 중심점 흔들림(px)
        float shotSpeedPxPerSec = 400.0f;   // 이 이상의 중심점 이동속도(px/s)를 샷으로 판정
        lli missToleranceUs = 150000;       // 공 검출이 잠깐(이 시간 이내) 끊겨도 상태를 리셋하지 않음
                                             // (정지된 공은 이벤트가 거의 없어 검출이 프레임마다 끊길 수 있음)
    };

    // 공 자동 인식 -> 레디 -> 샷 트리거 -> N초 촬영 상태 머신.
    // 매 처리 윈도우(EventProcessor::Process 결과)마다 Update()를 한 번 호출한다.
    class ShotTrigger
    {
    public:
        explicit ShotTrigger(const ShotTriggerConfig& config = ShotTriggerConfig());

        ShotUpdateResult Update(const BallDetectionResult& ball, lli nowUs);

        ShotState State() const { return m_state; }
        lli TriggerTimeUs() const { return m_triggerUs; }
        void Reset();

    private:
        ShotTriggerConfig m_config;
        ShotState m_state = ShotState::Searching;

        bool m_haveAnchor = false;
        cv::Point2f m_anchor;
        lli m_anchorStartUs = 0;

        bool m_havePrev = false;
        cv::Point2f m_prevCenter;
        lli m_prevUs = 0;
        lli m_lastSeenUs = 0;

        lli m_triggerUs = -1;
        lli m_captureStartUs = -1;
    };
}
