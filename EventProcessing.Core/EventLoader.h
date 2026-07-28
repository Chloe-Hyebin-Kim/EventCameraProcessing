#pragma once

#include "Event.h"

#include <string>
#include <vector>

namespace eventcore
{
    class EventLoader
    {
    public:
        static bool LoadFromCsv(const string& filePath, vector<Event>& events);
    };
}