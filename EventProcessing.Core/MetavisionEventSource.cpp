#include "pch.h"

#ifdef EVENTCORE_HAVE_METAVISION

#include "MetavisionEventSource.h"

#include <chrono>
#include <cstring>
#include <thread>

#include <metavision/sdk/base/events/event_cd.h>
#include <metavision/sdk/stream/camera.h>

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
        // Camera::from_file()/from_first_available()가 손상되었거나 지원되지 않는 RAW 파일 등에서
        // (문서화된 CameraException을 넘어) 실제 메모리 접근 위반을 던지는 경우가 확인됐다
        // (LiveEventStream.cpp와 동일한 문제). 정상적인 C++ 예외는 그대로 통과시켜 호출부의
        // catch(const std::exception&)가 원래 메시지를 잡게 하고, 그 외의 구조적 예외만 여기서
        // 막는다. MSVC(x64)는 __try가 있는 함수 안에 소멸자가 있는 C++ 지역 변수/임시 객체가
        // 있으면 컴파일을 거부하므로(C2712), __try 전용 함수는 함수 포인터 + void* context만
        // 다루는 순수 C 스타일로 분리한다 (LiveEventStream.cpp와 동일한 패턴).
        constexpr unsigned long kCxxExceptionCode = 0xE06D7363; // MSVC C++ 예외의 SEH 코드("msc")

        bool CallGuardedBySEH(void (*fn)(void*), void* context)
        {
            __try
            {
                fn(context);
                return true;
            }
            __except (GetExceptionCode() == kCxxExceptionCode ? EXCEPTION_CONTINUE_SEARCH : EXCEPTION_EXECUTE_HANDLER)
            {
                return false;
            }
        }

        struct OpenCameraContext
        {
            Metavision::Camera* target;
            bool live;
            const std::string* path;
        };

        // __try가 없는 평범한 함수라서, Camera의 값 대입/임시 객체가 있어도 문제없다.
        void DoOpenCamera(void* rawContext)
        {
            OpenCameraContext* ctx = static_cast<OpenCameraContext*>(rawContext);

            if (ctx->live)
            {
                *ctx->target = Metavision::Camera::from_first_available();
            }
            else
            {
                *ctx->target = Metavision::Camera::from_file(*ctx->path, Metavision::FileConfigHints().real_time_playback(false));
            }
        }
#endif
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

#if defined(_MSC_VER)
            const std::string pathStd = live ? std::string() : std::string(path);
            OpenCameraContext ctx{ &camera, live, &pathStd };

            if (!CallGuardedBySEH(&DoOpenCamera, &ctx))
            {
                std::cerr << "Failed to open Metavision event source '" << (path ? path : "<live>")
                    << "': SDK failed to open the source (possibly a corrupted or unsupported RAW file)" << std::endl;
                return false;
            }
#else
            if (live)
            {
                camera = Metavision::Camera::from_first_available();
            }
            else
            {
                camera = Metavision::Camera::from_file(std::string(path), Metavision::FileConfigHints().real_time_playback(false));
            }
#endif

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
