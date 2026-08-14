cmake_minimum_required(VERSION 3.16)
project(EventCameraProcessing VERSION 1.0 LANGUAGES CXX)

option(EVENTPROCESSING_WITH_METAVISION "Enable RAW and live-camera input with Metavision SDK" ON)
option(EVENTPROCESSING_BUILD_CONSOLE "Build the batch console application" ON)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)
set(CMAKE_AUTOMOC ON)

# The repository contains an OpenCV CMake package for the original Windows
# distribution. Prefer it on Windows while keeping the normal system-package
# lookup on Linux.
if(WIN32 AND EXISTS "${CMAKE_SOURCE_DIR}/ocv440/lib/opencv_world440.lib")
    add_library(opencv_world SHARED IMPORTED GLOBAL)
    set_target_properties(opencv_world PROPERTIES
        INTERFACE_INCLUDE_DIRECTORIES "${CMAKE_SOURCE_DIR}/ocv440/include"
        IMPORTED_CONFIGURATIONS "Debug;Release"
        IMPORTED_IMPLIB_DEBUG "${CMAKE_SOURCE_DIR}/ocv440/lib/opencv_world440d.lib"
        IMPORTED_LOCATION_DEBUG "${CMAKE_SOURCE_DIR}/opencv_world440d.dll"
        IMPORTED_IMPLIB_RELEASE "${CMAKE_SOURCE_DIR}/ocv440/lib/opencv_world440.lib"
        IMPORTED_LOCATION_RELEASE "${CMAKE_SOURCE_DIR}/opencv_world440.dll")
    set(OpenCV_LIBS opencv_world)
    set(EVENTPROCESSING_BUNDLED_OPENCV ON)
else()
    find_package(OpenCV 4 REQUIRED COMPONENTS core imgproc imgcodecs videoio)
endif()
find_package(QT NAMES Qt6 Qt5 REQUIRED COMPONENTS Widgets)
find_package(Qt${QT_VERSION_MAJOR} REQUIRED COMPONENTS Widgets)

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
target_link_libraries(CEventProcessingDiagDlg PRIVATE EventProcessingCore Qt${QT_VERSION_MAJOR}::Widgets)
if(WIN32)
    set_target_properties(CEventProcessingDiagDlg PROPERTIES WIN32_EXECUTABLE TRUE)
    if(EVENTPROCESSING_BUNDLED_OPENCV)
        add_custom_command(TARGET CEventProcessingDiagDlg POST_BUILD
            COMMAND ${CMAKE_COMMAND} -E copy_if_different
                "$<TARGET_FILE:opencv_world>" "$<TARGET_FILE_DIR:CEventProcessingDiagDlg>")
    endif()

    # A Qt executable is not portable by itself: Qt6Widgets/Gui/Core and the
    # qwindows platform plugin must be deployed beside it. Resolve the deploy
    # tool from the same Qt kit selected by find_package, avoiding PATH and
    # preventing accidental mixing of Qt installations.
    get_target_property(QT_QMAKE_EXECUTABLE Qt${QT_VERSION_MAJOR}::qmake IMPORTED_LOCATION)
    get_filename_component(QT_BIN_DIRECTORY "${QT_QMAKE_EXECUTABLE}" DIRECTORY)
    find_program(QT_WINDEPLOYQT_EXECUTABLE windeployqt HINTS "${QT_BIN_DIRECTORY}" NO_DEFAULT_PATH)
    if(NOT QT_WINDEPLOYQT_EXECUTABLE)
        message(FATAL_ERROR "windeployqt was not found in the selected Qt kit: ${QT_BIN_DIRECTORY}")
    endif()
    add_custom_command(TARGET CEventProcessingDiagDlg POST_BUILD
        COMMAND "${QT_WINDEPLOYQT_EXECUTABLE}"
            --no-translations
            "$<IF:$<CONFIG:Debug>,--debug,--release>"
            --dir "$<TARGET_FILE_DIR:CEventProcessingDiagDlg>"
            "$<TARGET_FILE:CEventProcessingDiagDlg>"
        COMMAND_EXPAND_LISTS
        COMMENT "Deploying the Qt runtime and qwindows platform plugin")
endif()

if(EVENTPROCESSING_BUILD_CONSOLE)
    add_executable(EventProcessingConsole EventProcessing.Console/EventProcessing.Console.cpp)
    target_link_libraries(EventProcessingConsole PRIVATE EventProcessingCore)
endif()

include(GNUInstallDirs)
install(TARGETS CEventProcessingDiagDlg RUNTIME DESTINATION ${CMAKE_INSTALL_BINDIR})
