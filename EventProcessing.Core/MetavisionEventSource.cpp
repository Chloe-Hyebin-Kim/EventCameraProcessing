#include "pch.h"

#ifdef EVENTCORE_HAVE_METAVISION

#include "MetavisionEventSource.h"

#include <chrono>
#include <cstring>
#include <thread>

#include <metavision/sdk/base/events/event_cd.h>
#include <metavision/sdk/stream/camera.h>

namespace eventcore
{
    namespace
    {
        bool IsLiveRequest(const char* path)
        {
            return path == nullptr || std::strlen(path) == 0 ||
                std::strcmp(path, "live") == 0 || std::strcmp(path, "camera") == 0;
        }
    }

    MetavisionEventSource::MetavisionEventSource(int64_t liveCaptureDurationUs)
        : m_liveCaptureDurationUs(liveCaptureDurationUs)
    {
    }

    MetavisionEventSource::~MetavisionEventSource() = default;

    bool MetavisionEventSource::Open(const char* path)
    {
        m_events.clear();
        m_firstUs = 0;
        m_lastUs = 0;

        const bool live = IsLiveRequest(path);

        try
        {
            Metavision::Camera camera;

            if (live)
            {
                camera = Metavision::Camera::from_first_available();
            }
            else
            {
                camera = Metavision::Camera::from_file(std::string(path), Metavision::FileConfigHints().real_time_playback(false));
            }

            camera.cd().add_callback([this](const Metavision::EventCD* begin, const Metavision::EventCD* end)
            {
                for (const Metavision::EventCD* it = begin; it != end; ++it)
                {
                    Event e;
                    e.t_us = it->t;
                    e.x = it->x;
                    e.y = it->y;
                    e.polarity = it->p ? 1 : -1;
                    m_events.push_back(e);
                }
            });

            camera.start();

            const auto captureStart = std::chrono::steady_clock::now();

            while (camera.is_running())
            {
                std::this_thread::sleep_for(std::chrono::milliseconds(10));

                if (live)
                {
                    const auto elapsedUs = std::chrono::duration_cast<std::chrono::microseconds>(
                        std::chrono::steady_clock::now() - captureStart).count();

                    if (elapsedUs >= m_liveCaptureDurationUs)
                    {
                        break;
                    }
                }
            }

            camera.stop();
        }
        catch (const std::exception& ex)
        {
            std::cerr << "Failed to open Metavision event source '" << (path ? path : "<live>") << "': " << ex.what() << std::endl;
            return false;
        }
        catch (...)
        {
            std::cerr << "Failed to open Metavision event source '" << (path ? path : "<live>") << "': unknown error" << std::endl;
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

    bool MetavisionEventSource::ReadEvents(std::vector<Event>& events, int64_t startUs, int64_t endUs)
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

    int MetavisionEventSource::Width() const
    {
        return m_width;
    }

    int MetavisionEventSource::Height() const
    {
        return m_height;
    }

    int64_t MetavisionEventSource::FirstTimestampUs() const
    {
        return m_firstUs;
    }

    int64_t MetavisionEventSource::LastTimestampUs() const
    {
        return m_lastUs;
    }
}

#endif // EVENTCORE_HAVE_METAVISION
