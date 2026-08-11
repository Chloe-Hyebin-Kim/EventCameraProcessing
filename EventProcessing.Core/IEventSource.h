#pragma once

#include "Event.h"

#include <cstdint>
#include <vector>

namespace eventcore
{
    class IEventSource
    {
    public:
        virtual ~IEventSource() = default;

        // path: CsvEventSource -> .csv file, MetavisionEventSource -> .raw/.hdf5 file or camera serial
        virtual bool Open(const char* path) = 0;

        // returns events with startUs <= t_us < endUs
        virtual bool ReadEvents(std::vector<Event>& events, int64_t startUs, int64_t endUs) = 0;

        virtual int Width() const = 0;
        virtual int Height() const = 0;

        // half-open range [FirstTimestampUs, LastTimestampUs] of loaded events, used to drive windowed playback/export
        virtual int64_t FirstTimestampUs() const = 0;
        virtual int64_t LastTimestampUs() const = 0;
    };
}