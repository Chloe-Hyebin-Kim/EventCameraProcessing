#pragma once

#include "IEventSource.h"

// EVK4 HD / IMX636 RAW/live 지원은 Prophesee Metavision SDK가 설치되어 있을 때만 컴파일된다.
// vcxproj에서 SDK가 설치된 환경에 한해 EVENTCORE_HAVE_METAVISION을 정의한다.
#ifdef EVENTCORE_HAVE_METAVISION

#include <cstdint>
#include <vector>

namespace eventcore
{
    // Prophesee Metavision SDK 기반 이벤트 소스.
    // - path가 .raw(.hdf5) 파일 경로면 해당 RAW 녹화본을 처음부터 끝까지 읽어들인다.
    // - path가 nullptr/빈 문자열/"live"/"camera"이면 연결된 EVK4 HD(IMX636) 카메라에서
    //   실시간으로 liveCaptureDurationUs 만큼 캡처한다.
    class MetavisionEventSource : public IEventSource
    {
    public:
        explicit MetavisionEventSource(int64_t liveCaptureDurationUs = 5000000);
        ~MetavisionEventSource() override;

        bool Open(const char* path) override;

        bool ReadEvents(std::vector<Event>& events, int64_t startUs, int64_t endUs) override;

        int Width() const override;
        int Height() const override;

        int64_t FirstTimestampUs() const override;
        int64_t LastTimestampUs() const override;

    private:
        int64_t m_liveCaptureDurationUs;
        std::vector<Event> m_events;
        int m_width = WIDTH;
        int m_height = HEIGHT;
        int64_t m_firstUs = 0;
        int64_t m_lastUs = 0;
    };
}

#endif // EVENTCORE_HAVE_METAVISION
