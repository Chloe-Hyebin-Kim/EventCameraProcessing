#include "pch.h"

#ifdef EVENTCORE_HAVE_METAVISION

#include "LiveEventStream.h"

#include <cstring>

#include <metavision/sdk/base/events/event_cd.h>
#include <metavision/sdk/stream/offline_streaming_control.h>

#if defined(_MSC_VER)
#include <excpt.h>
#endif

namespace eventcore
{
    namespace
    {
        bool IsLiveRequest(const char* path)
        {
            return path == nullptr || std::strlen(path) == 0 ||
                std::strcmp(path, "live") == 0 || std::strcmp(path, "camera") == 0;
        }

#if defined(_MSC_VER)
        // 카메라가 완전히 준비되기 전에 offline_streaming_control() 계열을 호출하면 Metavision
        // SDK가 문서화된 대로 CameraException을 던지기도 하지만, 실제로는 일반 catch(...)로
        // 못 잡는 메모리 접근 위반(구조적 예외)이 나는 경우도 확인됐다. MSVC에서는 SEH로 감싸서
        // 이런 경우에도 프로그램 전체가 죽지 않고 그 호출만 실패 처리되게 한다.
        template <typename Func>
        bool SafeCallBool(Func&& func)
        {
            __try
            {
                return func();
            }
            __except (EXCEPTION_EXECUTE_HANDLER)
            {
                return false;
            }
        }
#else
        template <typename Func>
        bool SafeCallBool(Func&& func)
        {
            try
            {
                return func();
            }
            catch (...)
            {
                return false;
            }
        }
#endif
    }

    LiveEventStream::LiveEventStream() = default;

    LiveEventStream::~LiveEventStream()
    {
        Stop();
    }

    bool LiveEventStream::Start(const char* path, lli windowUs, FrameCallback callback)
    {
        Stop();
        m_lastError.clear();

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
            m_lastError = ex.what();
            return false;
        }
        catch (...)
        {
            m_lastError = "unknown error opening source";
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
            m_lastError = ex.what();
            return false;
        }
        catch (...)
        {
            m_lastError = "unknown error starting camera";
            return false;
        }

        m_running = true;
        m_windowThread = std::thread(&LiveEventStream::WindowLoop, this, windowUs, callback);

        return true;
    }

    void LiveEventStream::Stop()
    {
        // m_running은 RAW 파일이 끝까지 재생되어 WindowLoop() 스스로 false로 바꾸는 경우도 있어서
        // (Stop()이 따로 호출되지 않은 채로), 이 값만 보고 일찍 return하면 안 된다. 그러면
        // m_windowThread가 join되지 않은 채로 남고, 다음 Start()에서 std::thread에 새 스레드를
        // move-assign할 때 여전히 joinable한 스레드가 남아있어 std::terminate()가 호출된다
        // (재생이 끝난 뒤 다른 RAW로 다시 Start했을 때 크래시하는 원인이었음).
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

    bool LiveEventStream::IsSeekable()
    {
        if (!m_running)
        {
            return false;
        }

        // Live 카메라 소스 등 offline streaming control이 없는 경우 SafeCallBool이 false를 반환한다.
        return SafeCallBool([this]()
        {
            return m_camera.offline_streaming_control().is_ready();
        });
    }

    bool LiveEventStream::GetSeekRange(lli& startUs, lli& endUs)
    {
        if (!IsSeekable())
        {
            return false;
        }

        lli localStartUs = 0;
        lli localEndUs = 0;

        const bool ok = SafeCallBool([this, &localStartUs, &localEndUs]()
        {
            Metavision::OfflineStreamingControl& osc = m_camera.offline_streaming_control();
            localStartUs = osc.get_seek_start_time();
            localEndUs = osc.get_seek_end_time();
            return true;
        });

        if (ok)
        {
            startUs = localStartUs;
            endUs = localEndUs;
        }

        return ok;
    }

    bool LiveEventStream::Seek(lli timestampUs)
    {
        if (!IsSeekable())
        {
            return false;
        }

        // 탐색 시점 전후로 섞인 이벤트가 다음 윈도우에 함께 들어가지 않도록 버퍼를 비운다.
        {
            std::lock_guard<std::mutex> lock(m_bufferMutex);
            m_buffer.clear();
        }

        return SafeCallBool([this, timestampUs]()
        {
            return m_camera.offline_streaming_control().seek(timestampUs);
        });
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
