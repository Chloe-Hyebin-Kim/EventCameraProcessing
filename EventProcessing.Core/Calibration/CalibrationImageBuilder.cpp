// 이 파일은 Calibration/ 하위 폴더에 있어 MSVC PCH(pch.h는 Core 루트)를 쓰지 않는다
// (.vcxproj에서 PrecompiledHeader=NotUsing). 필요한 헤더는 직접 include한다.
#include "CalibrationImageBuilder.h"

#include <algorithm>

namespace eventcore
{
    namespace
    {
        // accumulationUs가 0 이하로 들어오면(잘못된 설정) 이미지가 영영 완성되지 않으므로,
        // 최소 1 ms로 클램프한다. 상한은 두지 않는다(사용자가 아주 긴 노출을 원할 수 있음).
        lli SanitizeAccumulationUs(lli value)
        {
            constexpr lli kMinAccumulationUs = 1000;  // 1 ms
            return value >= kMinAccumulationUs ? value : kMinAccumulationUs;
        }
    }

    CalibrationImageBuilder::CalibrationImageBuilder(const CalibrationImageConfig& config, int width, int height)
        : m_config(config)
        , m_width(width > 0 ? width : 1)
        , m_height(height > 0 ? height : 1)
    {
        m_config.accumulationUs = SanitizeAccumulationUs(m_config.accumulationUs);

        m_posCount = cv::Mat::zeros(m_height, m_width, CV_32SC1);
        m_negCount = cv::Mat::zeros(m_height, m_width, CV_32SC1);
    }

    void CalibrationImageBuilder::Reset()
    {
        m_posCount.setTo(0);
        m_negCount.setTo(0);
        m_haveStart = false;
        m_startUs = 0;
        m_lastUs = 0;
    }

    void CalibrationImageBuilder::SetConfig(const CalibrationImageConfig& config)
    {
        m_config = config;
        m_config.accumulationUs = SanitizeAccumulationUs(m_config.accumulationUs);
        Reset();
    }

    lli CalibrationImageBuilder::CurrentSpanUs() const
    {
        return m_haveStart ? (m_lastUs - m_startUs) : 0;
    }

    bool CalibrationImageBuilder::AddEvents(const std::vector<Event>& events, lli windowStartUs, lli windowEndUs)
    {
        if (!m_haveStart)
        {
            m_startUs = windowStartUs;
            m_haveStart = true;
        }
        m_lastUs = windowEndUs;

        for (const Event& e : events)
        {
            if (e.x < 0 || e.x >= m_width || e.y < 0 || e.y >= m_height)
            {
                continue;
            }

            if (e.polarity > 0)
            {
                m_posCount.at<int>(e.y, e.x) += 1;
            }
            else
            {
                m_negCount.at<int>(e.y, e.x) += 1;
            }
        }

        if ((m_lastUs - m_startUs) >= m_config.accumulationUs)
        {
            Render();

            // 다음 이미지를 위한 누적 재시작(완성된 m_image는 유지).
            m_posCount.setTo(0);
            m_negCount.setTo(0);
            m_haveStart = false;

            return true;
        }

        return false;
    }

    void CalibrationImageBuilder::Render()
    {
        m_image.create(m_height, m_width, CV_8UC1);

        const uchar posV = static_cast<uchar>(std::clamp(m_config.positiveValue, 0, 255));
        const uchar negV = static_cast<uchar>(std::clamp(m_config.negativeValue, 0, 255));
        const uchar noneV = static_cast<uchar>(std::clamp(m_config.noEventValue, 0, 255));

        for (int y = 0; y < m_height; ++y)
        {
            const int* posRow = m_posCount.ptr<int>(y);
            const int* negRow = m_negCount.ptr<int>(y);
            uchar* dstRow = m_image.ptr<uchar>(y);

            for (int x = 0; x < m_width; ++x)
            {
                const int p = posRow[x];
                const int n = negRow[x];

                if (p > n)
                {
                    dstRow[x] = posV;
                }
                else if (n > p)
                {
                    dstRow[x] = negV;
                }
                else
                {
                    // p == n (둘 다 0 포함) -> 이벤트 없음/동수
                    dstRow[x] = noneV;
                }
            }
        }
    }
}
