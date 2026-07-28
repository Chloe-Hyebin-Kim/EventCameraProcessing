#pragma once

#include "Event.h"

#include <vector>

namespace eventcore
{
    class IEventSource
    {
    public:
        virtual ~IEventSource() = default;

        virtual bool Open(const char* path) = 0;

        virtual bool ReadEvents(std::vector<Event>& events, int64_t startUs, int64_t endUs) = 0;

        virtual int Width() const = 0;
        virtual int Height() const = 0;
    };
}