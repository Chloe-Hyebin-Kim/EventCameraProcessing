#include "pch.h"

#ifdef EVENTCORE_HAVE_METAVISION

#include "LiveEventStream.h"

#include <cstring>

#include <metavision/sdk/base/events/event_cd.h>

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

    LiveEventStream::LiveEventStream() = default;

    LiveEventStream::~LiveEventStream()
    {
        Stop();
    }

    bool LiveEventStream::Start(const char* path, lli windowUs, FrameCallback callback)
    {
        Stop();

        try
        {
            if (IsLiveRequest(path))
            {
                m_camera = Metavision::Camera::from_first_available();
            }
            else
            {
                m_camera = Metavision::Camera::from_file(std::string(path), Metavision::FileConfigHints().real_time_playback(true));
            }
        }
        catch (const std::exception& ex)
        {
            std::cerr << "LiveEventStream: failed to open '" << (path ? path : "<live>") << "': " << ex.what() << std::endl;
            return false;
        }

        {
            std::lock_guard<std::mutex> lock(m_bufferMutex);
            m_buffer.clear();
        }

        m_camera.cd().add_callback([this](const Metavision::EventCD* begin, const Metavision::EventCD* end)
        {
            std::lock_guard<std::mutex> lock(m_bufferMutex);
            for (const Metavision::EventCD* it = begin; it != end; ++it)
            {
                Event e;
                e.t_us = it->t;
                e.x = it->x;
                e.y = it->y;
                e.polarity = it->p ? 1 : -1;
                m_buffer.push_back(e);
            }
        });

        try
        {
            m_camera.start();
        }
        catch (const std::exception& ex)
        {
            std::cerr << "LiveEventStream: failed to start camera: " << ex.what() << std::endl;
            return false;
        }

        m_running = true;
        m_windowThread = std::thread(&LiveEventStream::WindowLoop, this, windowUs, callback);

        return true;
    }

    void LiveEventStream::Stop()
    {
        if (!m_running)
        {
            return;
        }

        m_running = false;

        if (m_windowThread.joinable())
        {
            m_windowThread.join();
        }

        try
        {
            m_camera.stop();
        }
        catch (...)
        {
        }
    }

    bool LiveEventStream::IsRunning() const
    {
        return m_running;
    }

    void LiveEventStream::WindowLoop(lli windowUs, FrameCallback callback)
    {
        lli runningClockUs = 0;

        while (m_running && m_camera.is_running())
        {
            std::this_thread::sleep_for(std::chrono::microseconds(windowUs));

            std::vector<Event> batch;
            {
                std::lock_guard<std::mutex> lock(m_bufferMutex);
                batch.swap(m_buffer);
            }

            const lli batchStart = batch.empty() ? runningClockUs : batch.front().t_us;
            const lli batchEnd = batch.empty() ? (runningClockUs + windowUs) : (batch.back().t_us + 1);
            runningClockUs = batchEnd;

            const EventProcessingResult result = EventProcessor::Process(batch, m_width, m_height, batchStart, batchEnd - batchStart);

            if (callback)
            {
                callback(result, batchStart, batchEnd);
            }
        }

        // RAW 파일이 끝까지 재생되어 카메라가 스스로 멈춘 경우(라이브가 아닌 경우)도 여기로 온다.
        m_running = false;
    }
}

#endif // EVENTCORE_HAVE_METAVISION
