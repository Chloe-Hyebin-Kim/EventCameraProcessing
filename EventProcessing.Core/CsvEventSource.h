#pragma once

#include "IEventSource.h"

#include <string>
#include <vector>

namespace eventcore
{
    // Loads a full t_us,x,y,p CSV file into memory. Used for offline testing
    // without a real EVK4 HD / IMX636 camera or RAW recording.
    class CsvEventSource : public IEventSource
    {
    public:
        bool Open(const char* path) override;

        bool ReadEvents(std::vector<Event>& events, int64_t startUs, int64_t endUs) override;

        int Width() const override;
        int Height() const override;

        int64_t FirstTimestampUs() const override;
        int64_t LastTimestampUs() const override;

    private:
        std::vector<Event> m_events;
        int m_width = WIDTH;
        int m_height = HEIGHT;
        int64_t m_firstUs = 0;
        int64_t m_lastUs = 0;
    };
}
