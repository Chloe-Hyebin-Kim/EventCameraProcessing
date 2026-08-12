cmake_minimum_required(VERSION 3.16)
project(EventCameraProcessing CXX)

# Linux(및 다른 CMake 지원 플랫폼)용 빌드.
#
#   EventProcessing.Core     - Static library, RAW/CSV/Live event processing 공통 로직
#   EventProcessing.Console  - Command-line batch processing (Metavision SDK 없이도 CSV만으로 빌드 가능)
#   EventProcessing.DiagQt   - Qt Widgets 기반 Live/RAW Diagnostic Viewer (Metavision SDK 필요)
#
# Windows에서는 기존과 동일하게 EventCameraProcessing.sln(MSBuild, MFC 기반 EventProcessing.Diag)을
# 사용해도 되고, Qt와 OpenCV를 준비했다면 이 CMake 빌드로 EventProcessing.DiagQt를 대신 빌드해도 된다.
#
# 필요 패키지 (Ubuntu/Debian 예):
#   sudo apt install cmake build-essential libopencv-dev qt6-base-dev
# Metavision SDK(Prophesee)는 https://docs.prophesee.ai 의 Linux 설치 안내를 따라 별도 설치한다.
# 설치되어 있으면 자동으로 감지되어 Live/RAW 지원이 켜지고, 없으면 CSV 입력만 지원하는 상태로
# EventProcessing.Core / EventProcessing.Console만 빌드된다(Windows의 Metavision.props와 동일한 동작).

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

if(NOT CMAKE_BUILD_TYPE AND NOT CMAKE_CONFIGURATION_TYPES)
    set(CMAKE_BUILD_TYPE Release CACHE STRING "Build type" FORCE)
endif()

find_package(OpenCV REQUIRED)

option(EVENTCORE_NO_METAVISION "Force-disable Metavision SDK support even if found" OFF)

set(EVENTCORE_HAVE_METAVISION OFF)
if(NOT EVENTCORE_NO_METAVISION)
    find_package(MetavisionSDK QUIET COMPONENTS base core stream)
    if(MetavisionSDK_FOUND)
        set(EVENTCORE_HAVE_METAVISION ON)
    endif()
endif()

if(EVENTCORE_HAVE_METAVISION)
    message(STATUS "Metavision SDK found - Live camera / RAW support enabled (EventProcessing.DiagQt included)")
else()
    message(STATUS "Metavision SDK not found - only CSV input supported; EventProcessing.DiagQt will NOT be built "
                    "(it requires real-time Live/RAW playback). EventProcessing.Console still builds.")
endif()

add_subdirectory(EventProcessing.Core)
add_subdirectory(EventProcessing.Console)

if(EVENTCORE_HAVE_METAVISION)
    add_subdirectory(EventProcessing.DiagQt)
endif()
