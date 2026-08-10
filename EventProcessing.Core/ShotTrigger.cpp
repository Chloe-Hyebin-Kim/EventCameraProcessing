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
        m_triggerUs = -1;
        m_captureStartUs = -1;
    }

    ShotUpdateResult ShotTrigger::Update(const BallDetectionResult& ball, lli nowUs)
    {
        ShotUpdateResult out;

        if (m_state == ShotState::Capturing)
        {
            const double elapsedSec = (nowUs - m_captureStartUs) / 1000000.0;

            if (elapsedSec >= m_config.captureSeconds)
            {
                m_state = ShotState::Searching;
                m_haveAnchor = false;
                m_havePrev = false;
                out.justFinishedCapture = true;
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
            out.state = m_state;
            return out;
        }

        m_lastSeenUs = nowUs;

        float speedPxPerSec = 0.0f;
        bool haveSpeed = false;

        if (m_havePrev && nowUs > m_prevUs)
        {
            const float dist = Distance(ball.center, m_prevCenter);
            const double dtSec = (nowUs - m_prevUs) / 1000000.0;
            speedPxPerSec = static_cast<float>(dist / dtSec);
            haveSpeed = true;
        }

        if (m_state == ShotState::Ready && haveSpeed && speedPxPerSec >= m_config.shotSpeedPxPerSec)
        {
            m_state = ShotState::Capturing;
            m_triggerUs = nowUs;
            m_captureStartUs = nowUs;
            out.justTriggered = true;
            out.state = m_state;

            m_havePrev = true;
            m_prevCenter = ball.center;
            m_prevUs = nowUs;

            return out;
        }

        if (!m_haveAnchor)
        {
            m_haveAnchor = true;
            m_anchor = ball.center;
            m_anchorStartUs = nowUs;
            m_state = ShotState::Searching;
        }
        else
        {
            const float distFromAnchor = Distance(ball.center, m_anchor);

            if (distFromAnchor > m_config.stableMovePx)
            {
                m_anchor = ball.center;
                m_anchorStartUs = nowUs;
                m_state = ShotState::Searching;
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
