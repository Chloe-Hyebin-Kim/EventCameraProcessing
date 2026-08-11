#include "pch.h"
#include "EventSourceFactory.h"

#include <cctype>

#include "CsvEventSource.h"

#ifdef EVENTCORE_HAVE_METAVISION
#include "MetavisionEventSource.h"
#endif

namespace eventcore
{
    namespace
    {
        std::string ToLowerExt(const std::string& path)
        {
            const size_t dot = path.find_last_of('.');

            if (dot == std::string::npos)
            {
                return "";
            }

            std::string ext = path.substr(dot);
            std::transform(ext.begin(), ext.end(), ext.begin(), [](unsigned char c) { return static_cast<char>(std::tolower(c)); });

            return ext;
        }
    }

    std::unique_ptr<IEventSource> EventSourceFactory::CreateForPath(const std::string& path)
    {
        const std::string ext = ToLowerExt(path);

        if (ext == ".csv")
        {
            return std::make_unique<CsvEventSource>();
        }

        if (path.empty() || path == "live" || path == "camera" || ext == ".raw" || ext == ".hdf5")
        {
#ifdef EVENTCORE_HAVE_METAVISION
            return std::make_unique<MetavisionEventSource>();
#else
            std::cerr << "This build has no Metavision SDK support (EVENTCORE_HAVE_METAVISION not defined); "
                "cannot open RAW/live source '" << path << "'." << std::endl;
            return nullptr;
#endif
        }

        std::cerr << "Unrecognized event source: '" << path << "' (expected .csv, .raw, .hdf5, or 'live')" << std::endl;
        return nullptr;
    }
}
