#pragma once

#include "IEventSource.h"

#include <memory>
#include <string>

namespace eventcore
{
    class EventSourceFactory
    {
    public:
        // ".csv" -> CsvEventSource, ".raw"/".hdf5" (or empty/"live"/"camera") -> MetavisionEventSource.
        // Returns nullptr if the extension is unrecognized, or if a RAW/live source is
        // requested but this build was compiled without EVENTCORE_HAVE_METAVISION.
        static std::unique_ptr<IEventSource> CreateForPath(const std::string& path);
    };
}
