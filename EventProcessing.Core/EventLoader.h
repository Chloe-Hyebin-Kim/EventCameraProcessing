#pragma once

#include "Event.h"

#include <string>
#include <vector>

namespace eventcore
{
    // CSV 파서 본체. CsvEventSource(IEventSource 구현체)가 내부적으로 사용한다.
    // 실제 EVK4 HD / IMX636 RAW 입력은 MetavisionEventSource(IEventSource 구현체)를 사용한다.
    class EventLoader
    {
    public:
        static bool LoadFromCsv(const std::string& filePath, std::vector<Event>& events);
    };
}