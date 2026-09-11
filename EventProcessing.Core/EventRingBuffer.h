#pragma once

// 프리트리거(pre-trigger) 이벤트 링버퍼.
//
// 왜 필요한가:
//   현재 MainWindow::StartCaptureSave()는 "트리거가 걸린 뒤부터" 저장을 시작한다.
//   그런데 측정 대상인 launch window는 임팩트 직후 4~8 ms 구간이다.
//   공이 움직이는 것을 보고 트리거가 걸린 시점에는 그 구간이 이미 지나가 있다.
//   게다가 저장하는 것이 debugImage(시각화 PNG)라서 원본 이벤트는 그 자리에서 사라지고,
//   파라미터를 바꿔 재분석할 방법이 없다.
//
//   그래서 최근 spanUs 만큼의 raw 이벤트를 항상 순환 저장해 두고,
//   트리거가 걸리면 "트리거 시점 이전"으로 되감아 덤프한다. 고속 촬영의 표준 구조다.
//
// 메모리:
//   Event 1개 = 24 B(패딩 포함). 기본값 maxEvents = 12,000,000 -> 약 288 MB.
//   버퍼는 첫 Push() 때 한 번만 할당되고 이후 재할당이 없다(링 구조).
//   실제 이벤트율에 맞춰 조절할 것. EVK4는 순간적으로 매우 높은 이벤트율을 낼 수 있으므로
//   ROI와 bias로 이벤트율을 먼저 낮추는 편이 좋다.
//
// 스레드:
//   이 클래스 자체는 동기화하지 않는다. LiveEventStream의 콜백 스레드에서 Push하고
//   다른 스레드에서 CopyRange 하려면 호출 측에서 뮤텍스를 잡아야 한다.
//   (ShotRecorder는 단일 스레드에서 쓰도록 되어 있다)

#include <cstddef>
#include <string>
#include <vector>

#include "Event.h"

namespace eventcore
{
    struct EventRingBufferConfig
    {
        lli spanUs = 300000;                // 보관할 시간 길이(us). 기본 300 ms
        std::size_t maxEvents = 12000000;   // 메모리 상한(이벤트 개수). 약 288 MB
    };

    class EventRingBuffer
    {
    public:
        EventRingBuffer();
        explicit EventRingBuffer(const EventRingBufferConfig& config);

        void Configure(const EventRingBufferConfig& config);   // 버퍼를 비우고 재설정한다
        const EventRingBufferConfig& Config() const { return m_config; }

        void Push(const Event& e);
        void Push(const std::vector<Event>& events);
        void Push(const Event* begin, const Event* end);

        void Clear();

        std::size_t Size() const { return m_count; }
        std::size_t Capacity() const { return m_buffer.size(); }
        bool Empty() const { return m_count == 0; }

        lli OldestUs() const;               // 비어 있으면 0
        lli NewestUs() const { return m_newestUs; }

        // 오래되어서(spanUs 초과) 버려진 것이 아니라, 메모리 상한 때문에 버려진 이벤트 수.
        // 이 값이 0보다 크면 maxEvents가 부족하다는 뜻이므로 반드시 로그로 확인할 것.
        std::size_t DroppedByCapacity() const { return m_droppedByCapacity; }

        // [fromUs, toUs) 구간의 이벤트를 시간순으로 복사한다. 반환값은 복사된 개수.
        std::size_t CopyRange(std::vector<Event>& out, lli fromUs, lli toUs) const;

        // 버퍼 전체를 시간순으로 복사한다.
        std::size_t CopyAll(std::vector<Event>& out) const;

        // 기존 CsvEventSource가 그대로 다시 읽을 수 있는 형식으로 저장한다.
        // 헤더 "t_us,x,y,p" + 행마다 "t,x,y,p" (p는 +1 / -1).
        // 즉 덤프한 파일을 EventProcessing.Console에 그대로 넣어 재처리할 수 있다.
        static bool WriteCsv(const std::string& path, const std::vector<Event>& events);

    private:
        void EnsureAllocated();
        void PopOldest();
        void TrimByAge();

        EventRingBufferConfig m_config;

        std::vector<Event> m_buffer;
        std::size_t m_head = 0;     // 가장 오래된 원소의 인덱스
        std::size_t m_count = 0;

        lli m_newestUs = 0;
        bool m_haveNewest = false;
        std::size_t m_droppedByCapacity = 0;
    };
}
