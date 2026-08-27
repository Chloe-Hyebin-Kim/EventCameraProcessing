set(EVENTCORE_SOURCES
    BallDetector.cpp
    CsvEventSource.cpp
    EventAccumulator.cpp
    EventFilter.cpp
    EventLoader.cpp
    EventProcessing.Core.cpp
    EventProcessor.cpp
    EventSourceFactory.cpp
    EventVisualizer.cpp
    ShotTrigger.cpp
    pch.cpp
)

set(EVENTCORE_HEADERS
    BallDetector.h
    CsvEventSource.h
    Event.h
    EventAccumulator.h
    EventFilter.h
    EventLoader.h
    EventProcessor.h
    EventSourceFactory.h
    EventVisualizer.h
    IEventSource.h
    ShotTrigger.h
    framework.h
    pch.h
)

if(EVENTCORE_HAVE_METAVISION)
    list(APPEND EVENTCORE_SOURCES
        LiveEventStream.cpp
        MetavisionEventSource.cpp
        MetavisionRuntime.cpp
    )
    list(APPEND EVENTCORE_HEADERS
        LiveEventStream.h
        MetavisionEventSource.h
        MetavisionRuntime.h
    )
endif()

add_library(EventProcessing.Core STATIC ${EVENTCORE_SOURCES} ${EVENTCORE_HEADERS})
add_library(EventProcessing::Core ALIAS EventProcessing.Core)

target_include_directories(EventProcessing.Core PUBLIC ${CMAKE_CURRENT_SOURCE_DIR})

target_link_libraries(EventProcessing.Core PUBLIC ${OpenCV_LIBS})
target_include_directories(EventProcessing.Core PUBLIC ${OpenCV_INCLUDE_DIRS})

if(EVENTCORE_HAVE_METAVISION)
    target_compile_definitions(EventProcessing.Core PUBLIC EVENTCORE_HAVE_METAVISION)
    target_link_libraries(EventProcessing.Core PUBLIC
        MetavisionSDK::base
        MetavisionSDK::core
        MetavisionSDK::stream
    )
    if(NOT WIN32)
        target_link_libraries(EventProcessing.Core PUBLIC ${CMAKE_DL_LIBS})
    endif()
endif()

set_target_properties(EventProcessing.Core PROPERTIES
    CXX_STANDARD 17
    CXX_STANDARD_REQUIRED ON
)
