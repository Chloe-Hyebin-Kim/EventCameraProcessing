cmake_minimum_required(VERSION 3.16)
project(EventCameraProcessing VERSION 1.0 LANGUAGES CXX)

option(EVENTPROCESSING_WITH_METAVISION "Enable RAW and live-camera input with Metavision SDK" ON)
option(EVENTPROCESSING_BUILD_CONSOLE "Build the batch console application" ON)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)
set(CMAKE_AUTOMOC ON)

find_package(OpenCV 4 REQUIRED COMPONENTS core imgproc imgcodecs videoio)
find_package(Qt5 5.12 REQUIRED COMPONENTS Widgets)

set(CORE_SOURCES
    EventProcessing.Core/BallDetector.cpp
    EventProcessing.Core/CsvEventSource.cpp
    EventProcessing.Core/EventAccumulator.cpp
    EventProcessing.Core/EventFilter.cpp
    EventProcessing.Core/EventLoader.cpp
    EventProcessing.Core/EventProcessor.cpp
    EventProcessing.Core/EventSourceFactory.cpp
    EventProcessing.Core/EventVisualizer.cpp
    EventProcessing.Core/LiveEventStream.cpp
    EventProcessing.Core/MetavisionEventSource.cpp
    EventProcessing.Core/MetavisionRuntime.cpp
    EventProcessing.Core/ShotTrigger.cpp)

add_library(EventProcessingCore STATIC ${CORE_SOURCES})
target_include_directories(EventProcessingCore PUBLIC EventProcessing.Core)
target_link_libraries(EventProcessingCore PUBLIC ${OpenCV_LIBS})
target_compile_features(EventProcessingCore PUBLIC cxx_std_17)

if(EVENTPROCESSING_WITH_METAVISION)
    find_path(METAVISION_INCLUDE_DIR metavision/sdk/stream/camera.h
        HINTS "$ENV{METAVISION_SDK_PATH}/include" "${CMAKE_SOURCE_DIR}/Prophesee/include")
    find_library(METAVISION_STREAM_LIBRARY metavision_sdk_stream
        HINTS "$ENV{METAVISION_SDK_PATH}/lib" "${CMAKE_SOURCE_DIR}/Prophesee/lib")
    find_library(METAVISION_CORE_LIBRARY metavision_sdk_core
        HINTS "$ENV{METAVISION_SDK_PATH}/lib" "${CMAKE_SOURCE_DIR}/Prophesee/lib")
    find_library(METAVISION_BASE_LIBRARY metavision_sdk_base
        HINTS "$ENV{METAVISION_SDK_PATH}/lib" "${CMAKE_SOURCE_DIR}/Prophesee/lib")
    if(METAVISION_INCLUDE_DIR AND METAVISION_STREAM_LIBRARY AND METAVISION_CORE_LIBRARY AND METAVISION_BASE_LIBRARY)
        target_compile_definitions(EventProcessingCore PUBLIC EVENTCORE_HAVE_METAVISION)
        target_include_directories(EventProcessingCore PUBLIC "${METAVISION_INCLUDE_DIR}")
        target_link_libraries(EventProcessingCore PUBLIC
            "${METAVISION_STREAM_LIBRARY}" "${METAVISION_CORE_LIBRARY}" "${METAVISION_BASE_LIBRARY}")
        message(STATUS "Metavision SDK support enabled")
    else()
        message(WARNING "Metavision SDK was not found; the GUI will build but RAW/live input will be unavailable")
    endif()
endif()

add_executable(CEventProcessingDiagDlg
    EventProcessing.Diag/EventProcessing.Diag.cpp
    EventProcessing.Diag/EventProcessingDiagDlg.cpp
    EventProcessing.Diag/EventProcessingDiagDlg.h)
target_link_libraries(CEventProcessingDiagDlg PRIVATE EventProcessingCore Qt5::Widgets)
if(WIN32)
    set_target_properties(CEventProcessingDiagDlg PROPERTIES WIN32_EXECUTABLE TRUE)
endif()

if(EVENTPROCESSING_BUILD_CONSOLE)
    add_executable(EventProcessingConsole EventProcessing.Console/EventProcessing.Console.cpp)
    target_link_libraries(EventProcessingConsole PRIVATE EventProcessingCore)
endif()

include(GNUInstallDirs)
install(TARGETS CEventProcessingDiagDlg RUNTIME DESTINATION ${CMAKE_INSTALL_BINDIR})
