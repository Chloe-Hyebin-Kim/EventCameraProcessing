#include "pch.h"
#include "CsvEventSource.h"

#include "EventLoader.h"

namespace eventcore
{
    bool CsvEventSource::Open(const char* path)
    {
        m_events.clear();
        m_firstUs = 0;
        m_lastUs = 0;

        if (!EventLoader::LoadFromCsv(path, m_events))
        {
            return false;
        }

        if (!m_events.empty())
        {
            m_firstUs = m_events.front().t_us;
            m_lastUs = m_events.front().t_us;

            for (const Event& e : m_events)
            {
                if (e.t_us < m_firstUs)
                {
                    m_firstUs = e.t_us;
                }

                if (e.t_us > m_lastUs)
                {
                    m_lastUs = e.t_us;
                }
            }
        }

        return true;
    }

    bool CsvEventSource::ReadEvents(std::vector<Event>& events, int64_t startUs, int64_t endUs)
    {
        events.clear();

        for (const Event& e : m_events)
        {
            if (e.t_us >= startUs && e.t_us < endUs)
            {
                events.push_back(e);
            }
        }

        return true;
    }

    int CsvEventSource::Width() const
    {
        return m_width;
    }

    int CsvEventSource::Height() const
    {
        return m_height;
    }

    int64_t CsvEventSource::FirstTimestampUs() const
    {
        return m_firstUs;
    }

    int64_t CsvEventSource::LastTimestampUs() const
    {
        return m_lastUs;
    }
}
