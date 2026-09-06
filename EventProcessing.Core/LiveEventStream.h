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

#include <metavision/sdk/stream/camera.h>

namespace eventcore
{
    // 실시간 라이브 카메라 또는 RAW 파일의 실시간(real_time_playback) 재생 스트림.
    // windowUs 간격(대략적인 화면 갱신 주기)마다 그 사이 수신된 이벤트를 EventProcessor::Process로
    // 누적/분석해 콜백으로 전달한다.
    //
    // 콜백은 내부 워커 스레드에서 호출된다. UI(Qt 등)를 갱신할 때는 콜백 안에서 직접 컨트롤을
    // 만지지 말고, PostMessage/QMetaObject::invokeMethod 등으로 UI 스레드에 마샬링해야 한다.
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

        // 재생을 그 자리에서 멈춘다(카메라 자체를 정지시키므로 이후 콜백이 더 이상 오지 않는다).
        // 워커 스레드는 join하지 않고 살려 둔 채로 대기시켜, Resume()이 이어서 재개할 수 있게 한다.
        // 주로 RAW 파일 재생을 "그 자리에서 멈춘 화면"으로 pause하는 용도(라이브 카메라에도 걸 수는
        // 있지만, 그러면 라이브 프리뷰 자체가 멈추므로 보통 라이브 모드에서는 쓰지 않는다).
        bool Pause();

        // Pause()로 멈춘 재생을 이어서 재개한다. 탐색 가능한 소스(RAW 파일)라면 멈췄던 바로 그
        // 시각으로 다시 seek해서, 멈춰 있던 동안 흐른 실제 시간만큼 건너뛰지 않고 그 자리에서부터
        // 이어서 재생되게 한다.
        bool Resume();

        bool IsPaused() const { return m_paused; }

        // 워커 스레드가 아직 돌고 있는지. RAW 파일이 끝까지 재생되면 스레드가 스스로 종료되며
        // 이 값이 false가 되므로, UI에서 주기적으로 폴링해 재생 종료를 감지할 수 있다.
        bool IsRunning() const;

        // RAW 파일 재생 중에만(Live 카메라 소스에서는 항상 false) 탐색(seek)이 가능하다.
        // RAW를 연 직후에는 SDK가 탐색 정보를 아직 준비하지 못했을 수 있으므로, true가 될 때까지
        // 주기적으로 확인해야 한다.
        bool IsSeekable();

        // [startUs, endUs] 탐색 가능 범위. IsSeekable()이 true일 때만 성공한다.
        bool GetSeekRange(lli& startUs, lli& endUs);

        // 지정한 시각으로 탐색한다(슬라이더 이동, 좌우 화살표 키를 이용한 되감기/앞으로 감기 등).
        // IsSeekable()이 true일 때만 성공한다.
        bool Seek(lli timestampUs);

        int Width() const { return m_width; }
        int Height() const { return m_height; }

        // Start()가 false를 반환했을 때, 실패 원인(SDK 예외 메시지 등)을 확인한다.
        const std::string& LastError() const { return m_lastError; }

    private:
        void WindowLoop(lli windowUs, FrameCallback callback);

        Metavision::Camera m_camera;
        std::thread m_windowThread;
        std::atomic<bool> m_running{ false };
        std::atomic<bool> m_paused{ false };

        // WindowLoop가 마지막으로 처리한 배치의 끝 시각. Resume()이 seek로 되돌아갈 지점.
        std::atomic<lli> m_lastProcessedUs{ 0 };

        std::mutex m_bufferMutex;
        std::vector<Event> m_buffer;

        int m_width = WIDTH;
        int m_height = HEIGHT;
        std::string m_lastError;
    };
}

#endif // EVENTCORE_HAVE_METAVISION
