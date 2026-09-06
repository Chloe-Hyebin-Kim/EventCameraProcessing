#include "pch.h"
#include "ShotTrigger.h"

#include <cmath>

namespace eventcore
{
    namespace
    {
        float Distance(const cv::Point2f& a, const cv::Point2f& b)
        {
            const float dx = a.x - b.x;
            const float dy = a.y - b.y;
            return std::sqrt(dx * dx + dy * dy);
        }

        // a, b 사이의 각도(도, 0~180). 둘 중 하나라도 거의 0벡터면(위치 잡음 수준의 미세 변위)
        // 방향을 정의할 수 없으므로 180(=최대 불일치)을 돌려준다.
        float AngleBetweenDeg(const cv::Point2f& a, const cv::Point2f& b)
        {
            const float na = std::sqrt(a.x * a.x + a.y * a.y);
            const float nb = std::sqrt(b.x * b.x + b.y * b.y);

            if (na < 1e-3f || nb < 1e-3f)
            {
                return 180.0f;
            }

            float cosAngle = (a.x * b.x + a.y * b.y) / (na * nb);
            cosAngle = std::clamp(cosAngle, -1.0f, 1.0f);

            return std::acos(cosAngle) * 180.0f / static_cast<float>(CV_PI);
        }
    }

    ShotTrigger::ShotTrigger(const ShotTriggerConfig& config)
        : m_config(config)
    {
    }

    void ShotTrigger::Reset()
    {
        m_state = ShotState::Searching;
        m_haveAnchor = false;
        m_havePrev = false;
        ResetCandidate();
        m_triggerUs = -1;
        m_captureStartUs = -1;
    }

    void ShotTrigger::ResetAnchor(const cv::Point2f& center, lli nowUs)
    {
        m_haveAnchor = true;
        m_anchor = center;
        m_anchorStartUs = nowUs;
        m_state = ShotState::Searching;
        ResetCandidate();
    }

    void ShotTrigger::ResetCandidate()
    {
        m_haveCandidate = false;
        m_haveLastDir = false;
        m_consistentCount = 0;
    }

    ShotUpdateResult ShotTrigger::Update(const BallDetectionResult& ball, lli nowUs)
    {
        ShotUpdateResult out;

        if (m_state == ShotState::Trajectory)
        {
            // postCaptureSeconds는 트리거된 프레임(m_captureStartUs, 즉 방향 일관성의 기준이 된
            // 가장 첫 프레임)부터 잰다. 이전 구간(preCaptureSeconds)은 이미 지나간 프레임이므로
            // 호출자(MainWindow)가 별도로 보관해 둔 프레임 버퍼에서 저장한다.
            const double elapsedSec = (nowUs - m_captureStartUs) / 1000000.0;

            if (elapsedSec >= m_config.postCaptureSeconds)
            {
                m_state = ShotState::Searching;
                m_haveAnchor = false;
                m_havePrev = false;
                ResetCandidate();
                out.justFinishedTrajectory = true;
            }

            out.state = m_state;
            return out;
        }

        if (!ball.detected)
        {
            // 정지된 공은 이벤트가 거의 없어 검출이 프레임마다 끊길 수 있으므로,
            // missToleranceUs 이내의 짧은 미검출은 상태를 리셋하지 않고 그대로 유지한다.
            if (m_haveAnchor && (nowUs - m_lastSeenUs) <= m_config.missToleranceUs)
            {
                out.state = m_state;
                return out;
            }

            m_haveAnchor = false;
            m_havePrev = false;
            m_state = ShotState::Searching;
            ResetCandidate();
            out.state = m_state;
            return out;
        }

        m_lastSeenUs = nowUs;

        float speedPxPerSec = 0.0f;
        bool haveSpeed = false;
        cv::Point2f dir(0.0f, 0.0f);

        if (m_havePrev && nowUs > m_prevUs)
        {
            dir = ball.center - m_prevCenter;
            const float dist = Distance(ball.center, m_prevCenter);
            const double dtSec = (nowUs - m_prevUs) / 1000000.0;
            speedPxPerSec = static_cast<float>(dist / dtSec);
            haveSpeed = true;
        }

        if (m_state == ShotState::Ready)
        {
            if (!m_haveCandidate)
            {
                // anchor(정지 위치)를 벗어난 첫 프레임을 "이동 후보"의 기준 프레임으로 잡는다.
                const float distFromAnchor = Distance(ball.center, m_anchor);

                if (distFromAnchor > m_config.stableMovePx)
                {
                    m_haveCandidate = true;
                    m_candidateStartUs = nowUs;
                    m_haveLastDir = false;
                    m_consistentCount = 0;
                }
            }
            else if (haveSpeed)
            {
                const float dist = Distance(ball.center, m_prevCenter);
                const bool haveDir = dist > m_config.stableMovePx * 0.5f;

                if (haveDir)
                {
                    // hadPrevDir가 false면 이번 벡터가 후보 구간의 첫 변위라 아직 비교할 대상이
                    // 없다는 뜻이다. 이 경우는 "일관성 없음"이 아니라 "아직 판단 불가"이므로
                    // 리셋하지 않고 그냥 m_lastDir만 채워 다음 프레임부터 비교를 시작한다.
                    const bool hadPrevDir = m_haveLastDir;
                    bool consistent = false;

                    if (hadPrevDir)
                    {
                        const float angleDeg = AngleBetweenDeg(dir, m_lastDir);
                        consistent = (angleDeg <= m_config.maxDirectionDeviationDeg)
                            && (speedPxPerSec >= m_config.shotSpeedPxPerSec);
                    }

                    m_lastDir = dir;
                    m_haveLastDir = true;

                    if (hadPrevDir && !consistent)
                    {
                        // 방향이 크게 어긋남(지그재그) -> 실제 샷이 아니라고 보고 처음부터 다시 관찰한다.
                        // 이 프레임을 새 anchor로 삼아 다시 readySeconds만큼 정지를 확인해야 한다.
                        ResetAnchor(ball.center, nowUs);
                        m_havePrev = true;
                        m_prevCenter = ball.center;
                        m_prevUs = nowUs;
                        out.state = m_state;
                        return out;
                    }

                    if (hadPrevDir && consistent)
                    {
                        ++m_consistentCount;

                        if (m_consistentCount >= m_config.directionConsistentFrames)
                        {
                            // 방향 일관성 확정 -> Impact. 저장 구간의 기준 시각은 확인이 끝난
                            // 지금이 아니라 이동이 시작된 "가장 첫 프레임"(m_candidateStartUs)이다.
                            m_triggerUs = m_candidateStartUs;
                            m_captureStartUs = m_candidateStartUs;
                            m_state = ShotState::Trajectory;
                            ResetCandidate();

                            out.justTriggered = true;
                            out.state = ShotState::Impact;

                            m_havePrev = true;
                            m_prevCenter = ball.center;
                            m_prevUs = nowUs;

                            return out;
                        }
                    }
                }
            }

            m_havePrev = true;
            m_prevCenter = ball.center;
            m_prevUs = nowUs;
            out.state = m_state;
            return out;
        }

        // Searching (또는 아직 Ready로 확정되지 않은 상태)에서의 anchor/정지 시간 판정.
        if (!m_haveAnchor)
        {
            ResetAnchor(ball.center, nowUs);
        }
        else
        {
            const float distFromAnchor = Distance(ball.center, m_anchor);

            if (distFromAnchor > m_config.stableMovePx)
            {
                ResetAnchor(ball.center, nowUs);
            }
            else
            {
                const double heldSec = (nowUs - m_anchorStartUs) / 1000000.0;

                if (heldSec >= m_config.readySeconds)
                {
                    if (m_state != ShotState::Ready)
                    {
                        out.justEnteredReady = true;
                    }

                    m_state = ShotState::Ready;
                }
                else
                {
                    m_state = ShotState::Searching;
                }
            }
        }

        m_havePrev = true;
        m_prevCenter = ball.center;
        m_prevUs = nowUs;

        out.state = m_state;
        return out;
    }
}
