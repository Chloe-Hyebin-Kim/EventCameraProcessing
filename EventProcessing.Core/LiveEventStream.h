#pragma once

#include "EventProcessor.h"

// 실시간 라이브 카메라 / RAW 실시간 재생 지원은 Metavision SDK가 있을 때만 컴파일된다.
#ifdef EVENTCORE_HAVE_METAVISION

#include <atomic>
#include <functional>
#include <mutex>
#include <string>
#include <thread>
#include <vector>

#include <metavision/sdk/driver/camera.h>

namespace eventcore
{
    // 실시간 라이브 카메라 또는 RAW 파일의 실시간(real_time_playback) 재생 스트림.
    // windowUs 간격(대략적인 화면 갱신 주기)마다 그 사이 수신된 이벤트를 EventProcessor::Process로
    // 누적/분석해 콜백으로 전달한다.
    //
    // 콜백은 내부 워커 스레드에서 호출된다. UI(MFC 등)를 갱신할 때는 콜백 안에서 직접 컨트롤을
    // 만지지 말고, PostMessage 등으로 UI 스레드에 마샬링해야 한다.
    class LiveEventStream
    {
    public:
        using FrameCallback = std::function<void(const EventProcessingResult& result, lli windowStartUs, lli windowEndUs)>;

        LiveEventStream();
        ~LiveEventStream();

        LiveEventStream(const LiveEventStream&) = delete;
        LiveEventStream& operator=(const LiveEventStream&) = delete;

        // path: nullptr/""/"live"/"camera" -> 연결된 카메라, 그 외 -> RAW 파일을 실시간 속도로 재생
        bool Start(const char* path, lli windowUs, FrameCallback callback);

        // 재생 중지(카메라 정지 + 워커 스레드 join). RAW 파일 재생이 끝까지 재생되어 자연 종료된
        // 경우에도 정리를 위해 호출해야 한다.
        void Stop();

        // 워커 스레드가 아직 돌고 있는지. RAW 파일이 끝까지 재생되면 스레드가 스스로 종료되며
        // 이 값이 false가 되므로, UI에서 주기적으로 폴링해 재생 종료를 감지할 수 있다.
        bool IsRunning() const;

        int Width() const { return m_width; }
        int Height() const { return m_height; }

    private:
        void WindowLoop(lli windowUs, FrameCallback callback);

        Metavision::Camera m_camera;
        std::thread m_windowThread;
        std::atomic<bool> m_running{ false };

        std::mutex m_bufferMutex;
        std::vector<Event> m_buffer;

        int m_width = WIDTH;
        int m_height = HEIGHT;
    };
}

#endif // EVENTCORE_HAVE_METAVISION
