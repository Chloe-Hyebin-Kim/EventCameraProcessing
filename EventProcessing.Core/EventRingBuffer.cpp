#include "pch.h"
#include "EventRingBuffer.h"

#include <charconv>
#include <fstream>
#include <iostream>

#include "Utf8Path.h"

namespace eventcore
{
    EventRingBuffer::EventRingBuffer() = default;

    EventRingBuffer::EventRingBuffer(const EventRingBufferConfig& config)
        : m_config(config)
    {
    }

    void EventRingBuffer::Configure(const EventRingBufferConfig& config)
    {
        m_config = config;

        m_buffer.clear();
        m_buffer.shrink_to_fit();

        Clear();
    }

    void EventRingBuffer::Clear()
    {
        m_head = 0;
        m_count = 0;
        m_newestUs = 0;
        m_haveNewest = false;
        m_droppedByCapacity = 0;
    }

    void EventRingBuffer::EnsureAllocated()
    {
        if (!m_buffer.empty())
        {
            return;
        }

        std::size_t capacity = m_config.maxEvents;

        if (capacity == 0)
        {
            capacity = 1;
        }

        // 링버퍼는 재할당이 없어야 의미가 있으므로 한 번에 전부 확보한다.
        m_buffer.resize(capacity);
    }

    void EventRingBuffer::PopOldest()
    {
        if (m_count == 0)
        {
            return;
        }

        m_head = (m_head + 1) % m_buffer.size();
        --m_count;
    }

    void EventRingBuffer::TrimByAge()
    {
        if (m_config.spanUs <= 0 || !m_haveNewest)
        {
            return;
        }

        while (m_count > 0)
        {
            const Event& oldest = m_buffer[m_head];

            if (m_newestUs - oldest.t_us <= m_config.spanUs)
            {
                break;
            }

            PopOldest();
        }
    }

    void EventRingBuffer::Push(const Event& e)
    {
        EnsureAllocated();

        const std::size_t capacity = m_buffer.size();

        if (m_count == capacity)
        {
            // 메모리 상한에 걸려 가장 오래된 것을 버린다. 나중에 진단할 수 있도록 센다.
            PopOldest();
            ++m_droppedByCapacity;
        }

        const std::size_t slot = (m_head + m_count) % capacity;
        m_buffer[slot] = e;
        ++m_count;

        if (!m_haveNewest || e.t_us > m_newestUs)
        {
            m_newestUs = e.t_us;
            m_haveNewest = true;
        }

        TrimByAge();
    }

    void EventRingBuffer::Push(const Event* begin, const Event* end)
    {
        for (const Event* it = begin; it != end; ++it)
        {
            Push(*it);
        }
    }

    void EventRingBuffer::Push(const std::vector<Event>& events)
    {
        if (events.empty())
        {
            return;
        }

        Push(events.data(), events.data() + events.size());
    }

    lli EventRingBuffer::OldestUs() const
    {
        if (m_count == 0)
        {
            return 0;
        }

        return m_buffer[m_head].t_us;
    }

    std::size_t EventRingBuffer::CopyRange(std::vector<Event>& out, lli fromUs, lli toUs) const
    {
        out.clear();

        if (m_count == 0 || toUs <= fromUs)
        {
            return 0;
        }

        out.reserve(m_count);

        const std::size_t capacity = m_buffer.size();

        // 선형 스캔. SDK 콜백 경계에서 타임스탬프가 완전히 단조롭지 않을 수 있어
        // 이분탐색 대신 전수 검사를 한다. 샷당 한 번만 호출되므로 비용은 문제되지 않는다.
        for (std::size_t i = 0; i < m_count; ++i)
        {
            const Event& e = m_buffer[(m_head + i) % capacity];

            if (e.t_us >= fromUs && e.t_us < toUs)
            {
                out.push_back(e);
            }
        }

        return out.size();
    }

    std::size_t EventRingBuffer::CopyAll(std::vector<Event>& out) const
    {
        out.clear();

        if (m_count == 0)
        {
            return 0;
        }

        out.reserve(m_count);

        const std::size_t capacity = m_buffer.size();

        for (std::size_t i = 0; i < m_count; ++i)
        {
            out.push_back(m_buffer[(m_head + i) % capacity]);
        }

        return out.size();
    }

    bool EventRingBuffer::WriteCsv(const std::string& path, const std::vector<Event>& events)
    {
        std::ofstream file(Utf8ToPath(path), std::ios::binary);

        if (!file.is_open())
        {
            std::cerr << "EventRingBuffer::WriteCsv: failed to open " << path << std::endl;
            return false;
        }

        file << "t_us,x,y,p\n";

        // ofstream의 << 연산자는 수백만 행에서 매우 느리다. to_chars로 직접 만들어
        // 큰 덩어리로 한 번에 내보낸다.
        std::string chunk;
        chunk.reserve(1 << 20);

        char scratch[64];

        for (const Event& e : events)
        {
            auto append = [&](long long value)
            {
                const auto res = std::to_chars(scratch, scratch + sizeof(scratch), value);
                chunk.append(scratch, static_cast<std::size_t>(res.ptr - scratch));
            };

            append(static_cast<long long>(e.t_us));
            chunk.push_back(',');
            append(static_cast<long long>(e.x));
            chunk.push_back(',');
            append(static_cast<long long>(e.y));
            chunk.push_back(',');
            append(static_cast<long long>(e.polarity >= 0 ? 1 : -1));
            chunk.push_back('\n');

            if (chunk.size() >= (1u << 20))
            {
                file.write(chunk.data(), static_cast<std::streamsize>(chunk.size()));
                chunk.clear();
            }
        }

        if (!chunk.empty())
        {
            file.write(chunk.data(), static_cast<std::streamsize>(chunk.size()));
        }

        file.flush();

        return file.good();
    }
}
