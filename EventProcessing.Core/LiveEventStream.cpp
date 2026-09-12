#include "pch.h"

#ifdef EVENTCORE_HAVE_METAVISION

#include "LiveEventStream.h"

#include <cstring>

#include <metavision/sdk/base/events/event_cd.h>
#include <metavision/sdk/stream/offline_streaming_control.h>
#include <metavision/hal/facilities/i_ll_biases.h>

#if defined(_MSC_VER)
#include <excpt.h>
#endif

#include "Utf8Path.h"

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

#if defined(_MSC_VER)
        // Camera::from_file()/from_first_available()가 손상되었거나 지원되지 않는 RAW 파일 등에서
        // (문서화된 CameraException을 넘어) 실제 메모리 접근 위반을 던지는 경우가 확인됐다.
        // 정상적인 C++ 예외(CameraException 등)는 그대로 통과시켜 호출부의 catch(const
        // std::exception&)가 원래 메시지를 잡게 하고, 그 외의 구조적 예외(액세스 위반 등)만
        // 여기서 막아서 앱 전체가 죽는 대신 Start()가 실패로 처리되게 한다.
        //
        // MSVC(x64)는 __try가 있는 함수 안에 소멸자가 있는 C++ 지역 변수/임시 객체가 하나라도
        // 있으면 컴파일을 거부한다(C2712). Metavision::Camera를 값으로 반환/대입하면 바로 그런
        // 임시 객체가 생기므로, __try 전용 함수는 함수 포인터 + void* context만 다루는 순수
        // C 스타일로 완전히 분리하고, 실제 Camera를 다루는 코드는 __try가 없는 별도 함수에 둔다.
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
                *ctx->target = Metavision::Camera::from_file(Utf8ToPath(*ctx->path), Metavision::FileConfigHints().real_time_playback(true));
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
#if defined(_MSC_VER)
            const std::string pathStd = IsLiveRequest(path) ? std::string() : std::string(path);
            OpenCameraContext ctx{ &m_camera, IsLiveRequest(path), &pathStd };

            if (!CallGuardedBySEH(&DoOpenCamera, &ctx))
            {
                m_lastError = "Metavision SDK failed to open the source (possibly a corrupted or unsupported RAW file)";
                return false;
            }
#else
            if (IsLiveRequest(path))
            {
                m_camera = Metavision::Camera::from_first_available();
            }
            else
            {
                m_camera = Metavision::Camera::from_file(Utf8ToPath(path), Metavision::FileConfigHints().real_time_playback(true));
            }
#endif
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
        m_paused = false;

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

        // m_camera.stop()은 이벤트 스트리밍만 멈출 뿐, 이 Camera 객체는 그대로 살아 있어서
        // 내부 장치(USB) 핸들을 계속 점유한다. 그 상태로 다시 Start()하면 Camera::from_first_available()가
        // (대입보다 우변이 먼저 평가되므로 아직 예전 핸들이 살아있는 채로) 장치를 열지 못해
        // "camera not found / not accessible"로 실패한다. 빈 Camera로 move-대입해 기존 객체를
        // 파괴(=장치 핸들 해제)시켜서, Start->Stop->Start가 정상 동작하게 한다.
        try
        {
            m_camera = Metavision::Camera();
        }
        catch (...)
        {
        }
    }

    bool LiveEventStream::Pause()
    {
        if (!m_running || m_paused)
        {
            return false;
        }

        // WindowLoop 스레드는 join하지 않고 그대로 둔다: m_paused만 세우면 다음 루프에서
        // 스스로 대기 상태로 들어가고, m_running은 그대로 true라 자연 종료(EOF) 처리 경로와
        // 헷갈리지 않는다.
        m_paused = true;

        try
        {
            m_camera.stop();
        }
        catch (...)
        {
            // 카메라가 멈추지 않아도(예: 이미 EOF로 멈춰 있던 경우) pause 자체는 유지한다.
        }

        return true;
    }

    bool LiveEventStream::Resume()
    {
        if (!m_running || !m_paused)
        {
            return false;
        }

        bool started = false;

        try
        {
            started = m_camera.start();
        }
        catch (...)
        {
            started = false;
        }

        if (!started)
        {
            return false;
        }

        // 탐색 가능한 소스(RAW 파일)라면 멈췄던 시각으로 되돌려서, pause 동안 흐른 실제 시간만큼
        // 앞으로 건너뛰지 않고 그 자리에서 이어서 재생되게 한다. 라이브 카메라는 애초에 이 함수로
        // pause하지 않으므로(호출부에서 라이브는 카메라를 세우지 않는 별도 경로를 씀) 보통 seek
        // 불가능 소스라 이 블록이 실행되지 않는다.
        if (IsSeekable())
        {
            SafeCallBool([this]()
            {
                return m_camera.offline_streaming_control().seek(m_lastProcessedUs.load());
            });
        }

        {
            std::lock_guard<std::mutex> lock(m_bufferMutex);
            m_buffer.clear();
        }

        m_paused = false;

        return true;
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

    std::vector<BiasSetting> LiveEventStream::GetBiases() const
    {
        std::vector<BiasSetting> result;

        if (!m_running)
        {
            return result;
        }

        // Camera::get_facility<T>()는 포인터가 아니라 참조를 반환하며, 해당 파실리티가 없는
        // 소스(RAW 파일 재생 등)에서는 CameraException(UnsupportedFeature)을 던진다.
        try
        {
            const Metavision::I_LL_Biases& biases = m_camera.get_facility<Metavision::I_LL_Biases>();
            const std::map<std::string, int> allBiases = biases.get_all_biases();

            for (const auto& [name, value] : allBiases)
            {
                BiasSetting setting;
                setting.name = name;
                setting.value = value;

                Metavision::LL_Bias_Info info;
                if (biases.get_bias_info(name, info))
                {
                    const std::pair<int, int> range = info.get_bias_range();
                    setting.minValue = range.first;
                    setting.maxValue = range.second;
                    setting.description = info.get_description();
                    setting.modifiable = info.is_modifiable();
                }

                result.push_back(std::move(setting));
            }
        }
        catch (...)
        {
            result.clear();
        }

        return result;
    }

    bool LiveEventStream::SetBias(const std::string& biasName, int value)
    {
        if (!m_running)
        {
            return false;
        }

        try
        {
            Metavision::I_LL_Biases& biases = m_camera.get_facility<Metavision::I_LL_Biases>();
            return biases.set(biasName, value);
        }
        catch (...)
        {
            return false;
        }
    }

    void LiveEventStream::WindowLoop(lli windowUs, FrameCallback callback)
    {
        lli runningClockUs = 0;

        while (m_running)
        {
            if (m_paused)
            {
                // Pause()가 카메라를 세워 뒀다. 스레드는 죽이지 않고 짧게 자며 대기하다가,
                // Resume()이 m_paused를 내리거나 Stop()이 m_running을 내리면 빠져나온다.
                std::this_thread::sleep_for(std::chrono::milliseconds(20));
                continue;
            }

            if (!m_camera.is_running())
            {
                break;
            }

            std::this_thread::sleep_for(std::chrono::microseconds(windowUs));

            std::vector<Event> batch;
            {
                std::lock_guard<std::mutex> lock(m_bufferMutex);
                batch.swap(m_buffer);
            }

            const lli batchStart = batch.empty() ? runningClockUs : batch.front().t_us;
            const lli batchEnd = batch.empty() ? (runningClockUs + windowUs) : (batch.back().t_us + 1);
            runningClockUs = batchEnd;
            m_lastProcessedUs = batchEnd;

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
