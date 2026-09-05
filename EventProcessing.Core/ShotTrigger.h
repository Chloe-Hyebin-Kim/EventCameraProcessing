#pragma once

#include "BallDetector.h"
#include "Event.h"

namespace eventcore
{
    enum class ShotState
    {
        Searching,  // 공이 없거나, 있어도 아직 정지 상태가 N초 이상 확인되지 않음
        Ready,      // 같은 위치(stableMovePx 이내)에서 readySeconds 이상 유지됨 -> 레디 신호
        Impact,     // Ready 상태에서 방향 일관성 있는 이동이 확인되어 샷으로 트리거된 바로 그 프레임
        Trajectory, // Impact 판정의 기준이 된 첫 프레임 전/후 구간을 저장하는 중
    };

    struct ShotUpdateResult
    {
        ShotState state = ShotState::Searching;
        bool justEnteredReady = false;      // 이번 Update에서 처음 Ready로 전이함
        bool justTriggered = false;         // 이번 Update에서 샷이 트리거됨(state == Impact)
        bool justFinishedTrajectory = false;// 이번 Update에서 postCaptureSeconds가 끝나 Searching으로 복귀
    };

    struct ShotTriggerConfig
    {
        double readySeconds = 1.0;          // N1: 레디로 인정하기까지 정지 유지 시간(초)
        double preCaptureSeconds = 2.0;     // N2: Impact 기준 프레임 이전 저장 구간(초)
        double postCaptureSeconds = 2.0;    // N3: Impact 기준 프레임 이후 저장 구간(초)
        float stableMovePx = 6.0f;          // 레디 판정 중 허용하는 중심점 흔들림(px)
        float shotSpeedPxPerSec = 400.0f;   // 이 이상의 중심점 이동속도(px/s)라야 "이동 후보"로 본다
        int directionConsistentFrames = 2;  // 방향 일관성 확인을 위해 연속으로 요구하는 구간(변위 벡터쌍) 수
        float maxDirectionDeviationDeg = 35.0f; // 연속 구간 사이에 허용하는 이동 방향 편차(도). 이보다 크면 지그재그로 간주해 리셋
        lli missToleranceUs = 150000;       // 공 검출이 잠깐(이 시간 이내) 끊겨도 상태를 리셋하지 않음
                                             // (정지된 공은 이벤트가 거의 없어 검출이 프레임마다 끊길 수 있음)
    };

    // 공 자동 인식 -> 레디 -> (방향 일관성 기반) 샷 트리거 -> 트리거 기준 프레임 전/후 구간 촬영
    // 상태 머신. 매 처리 윈도우(EventProcessor::Process 결과)마다 Update()를 한 번 호출한다.
    //
    // 트리거는 단순 프레임간 속도 임계값이 아니라, Ready 상태에서 공이 정지 위치를 벗어난
    // 첫 프레임부터 시작해 연속된 이동 벡터들의 방향이 서로 크게 어긋나지 않는지(지그재그가
    // 아닌지)를 directionConsistentFrames 구간 동안 확인한 뒤에야 확정한다. 확정되면
    // TriggerTimeUs()는 그 "가장 첫 프레임"의 타임스탬프를 가리킨다(확인이 끝난 프레임이 아님).
    class ShotTrigger
    {
    public:
        explicit ShotTrigger(const ShotTriggerConfig& config = ShotTriggerConfig());

        ShotUpdateResult Update(const BallDetectionResult& ball, lli nowUs);

        ShotState State() const { return m_state; }
        lli TriggerTimeUs() const { return m_triggerUs; }
        void Reset();

    private:
        void ResetAnchor(const cv::Point2f& center, lli nowUs);
        void ResetCandidate();

        ShotTriggerConfig m_config;
        ShotState m_state = ShotState::Searching;

        bool m_haveAnchor = false;
        cv::Point2f m_anchor;
        lli m_anchorStartUs = 0;

        bool m_havePrev = false;
        cv::Point2f m_prevCenter;
        lli m_prevUs = 0;
        lli m_lastSeenUs = 0;

        // Ready 상태에서 공이 anchor를 벗어난 뒤, 방향 일관성이 확정되기 전까지의 "이동 후보" 추적.
        bool m_haveCandidate = false;
        lli m_candidateStartUs = 0;      // 후보 이동의 가장 첫 프레임 시각(트리거되면 그대로 기준 시각이 됨)
        bool m_haveLastDir = false;
        cv::Point2f m_lastDir;           // 직전 구간의 이동 방향(단위 벡터)
        int m_consistentCount = 0;       // 연속으로 방향이 일관되게 확인된 구간 수

        lli m_triggerUs = -1;
        lli m_captureStartUs = -1;       // Trajectory 상태의 기준 시각(m_triggerUs와 동일)
    };
}
