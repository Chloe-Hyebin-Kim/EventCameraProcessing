#pragma once

#include "Event.h"

#include <string>
#include <vector>

namespace eventcore
{
    class EventLoader
    {
    public:

    //임시
        static bool LoadFromCsv(const std::string& filePath, std::vector<Event>& events);
    
    //추후 Metavision SDK 추가 
        // IEventSource
        // CsvEventSource -> 테스트용
        // MetavisionEventSource ->실제 RAW,camera stream용
    
    };
}