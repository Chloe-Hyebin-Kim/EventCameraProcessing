cmake_minimum_required(VERSION 3.16)
project(EventCameraProcessing CXX)

# Windows / Linux 공용 빌드. GUI는 Qt Widgets 하나로 통일되어 있어 이 CMake 빌드가
# 두 플랫폼 모두에서 EventProcessing.DiagQt(Diagnostic Viewer)를 만드는 유일한 방법이다.
# (과거 Windows 전용 MFC 버전 EventProcessing.Diag는 제거됨.)
#
#   EventProcessing.Core     - Static library, RAW/CSV/Live event processing 공통 로직
#   EventProcessing.Console  - Command-line batch processing (Metavision SDK 없이도 CSV만으로 빌드 가능)
#   EventProcessing.DiagQt   - Qt Widgets 기반 Live/RAW Diagnostic Viewer (Metavision SDK 필요)
#
# Windows에서 EventProcessing.Core / EventProcessing.Console만 필요하면 기존처럼
# EventCameraProcessing.sln(MSBuild)을 그대로 써도 된다. EventProcessing.DiagQt(GUI)는
# Windows에서도 이 CMake 빌드로만 만들 수 있다 (Qt 설치 필요, 예: Qt Online Installer로
# MSVC 2022 64bit 키트 설치 후 -DCMAKE_PREFIX_PATH=<Qt 설치 경로> 지정, 또는 QT_DIR/QTDIR
# 환경 변수만 설정해두면 아래에서 자동으로 잡음. OpenCV는 리포에 번들된 ocv440\ + 루트의
# opencv_world440(d).dll을 아래에서 자동으로 찾아 쓰므로 별도 옵션 지정 불필요).
#
# 필요 패키지 (Ubuntu/Debian 예):
#   sudo apt install cmake build-essential libopencv-dev qt6-base-dev
# Metavision SDK(Prophesee)는 https://docs.prophesee.ai 의 설치 안내(Windows/Linux)를 따라 별도 설치한다.
# 설치되어 있으면 자동으로 감지되어 Live/RAW 지원이 켜지고, 없으면 CSV 입력만 지원하는 상태로
# EventProcessing.Core / EventProcessing.Console만 빌드된다(기존 Metavision.props와 동일한 동작).

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

if(NOT CMAKE_BUILD_TYPE AND NOT CMAKE_CONFIGURATION_TYPES)
    set(CMAKE_BUILD_TYPE Release CACHE STRING "Build type" FORCE)
endif()

# Visual Studio의 CMakePresets.json 구성 선택(툴바 드롭다운)은 VS 버전에 따라 신뢰할 수 없어서
# (예: VS2019가 CMAKE_PREFIX_PATH가 없는 다른 구성으로 조용히 configure해버리는 경우),
# 어떤 구성이 선택되든 Windows에서 Qt를 최대한 자동으로 찾도록 여기서 보강한다.
if(WIN32)
    # Qt 설치 경로: QT_DIR(권장) 또는 Qt 설치 관례상의 QTDIR 환경 변수를 검색 경로에 추가.
    if(DEFINED ENV{QT_DIR} AND NOT "$ENV{QT_DIR}" STREQUAL "")
        list(APPEND CMAKE_PREFIX_PATH "$ENV{QT_DIR}")
    elseif(DEFINED ENV{QTDIR} AND NOT "$ENV{QTDIR}" STREQUAL "")
        list(APPEND CMAKE_PREFIX_PATH "$ENV{QTDIR}")
    endif()
endif()

# 리포에 번들된 ocv440\lib\*.cmake는 표준 OpenCV Windows 배포판(원래 <pkg>\build\x64\vc16\lib\
# 아래에 있던 OpenCVConfig.cmake 등)에서 include\, lib\만 평평하게(flatten) 복사해온 것이다.
# 그 파일들 안의 상대 경로 계산이 원래의 4단계 중첩 구조(build\x64\vc16\lib\)를 가정하고 있어서,
# 이 리포의 평평한 구조(ocv440\lib\)에서는 엉뚱한 경로(리포 폴더 밖)를 가리키는 버그가 있었다.
# ocv440\lib\OpenCVConfig.cmake / OpenCVModules*.cmake 파일들 자체를 리포 실제 구조에 맞게
# 고쳐뒀으므로(주석 참고), 여기서는 그 경로를 CMAKE_PREFIX_PATH에 추가하기만 하면 된다.
# 이렇게 실제 OpenCVConfig.cmake를 거쳐야 opencv_core/opencv_imgproc/... 타깃이 만들어지고,
# Metavision SDK의 CMake 설정(MetavisionSDK_coreConfig.cmake)이 내부적으로 하는 자체
# find_package(OpenCV ...) 호출도 (우리가 아니라 그쪽이 직접 호출하는 것이므로) 이 경로를
# 통해서만 성공할 수 있다.
if(WIN32 AND EXISTS "${CMAKE_SOURCE_DIR}/ocv440/lib/OpenCVConfig.cmake")
    list(APPEND CMAKE_PREFIX_PATH "${CMAKE_SOURCE_DIR}/ocv440/lib")
endif()

find_package(OpenCV REQUIRED)

# Windows: 리포에 번들된 Prophesee\ (Metavision.props가 MSBuild 빌드에서 쓰는 것과 동일한 폴더)에
# CMake package config가 이미 들어있으므로(share\cmake\MetavisionSDK\...), 시스템에 SDK가 따로
# 설치되어 있지 않아도 이 리포만 clone하면 바로 잡히도록 검색 경로 맨 앞에 추가한다.
# Metavision.props와 동일하게 리포에 번들된 쪽을 시스템 설치보다 우선시킨다.
if(WIN32 AND EXISTS "${CMAKE_SOURCE_DIR}/Prophesee/share/cmake/MetavisionSDK/MetavisionSDKConfig.cmake")
    list(PREPEND CMAKE_PREFIX_PATH "${CMAKE_SOURCE_DIR}/Prophesee")
endif()

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

# Windows에서 리포에 번들된 Prophesee\, opencv_world440(d).dll을 그대로 쓰는 경우, 기존
# vcxproj의 Post-Build Copy Step(Metavision.props 문서 참고)과 동일하게 실행 파일 옆에
# 런타임 DLL/HAL 플러그인을 복사해 System PATH 설정 없이 바로 실행되게 한다.
# (system-wide Metavision SDK/OpenCV를 쓰는 경우는 각자 PATH에 있으므로 없어도 무방.)
function(eventcore_copy_windows_runtime_deps target)
    if(NOT WIN32)
        return()
    endif()

    if(EXISTS "${CMAKE_SOURCE_DIR}/Prophesee/bin")
        add_custom_command(TARGET ${target} POST_BUILD
            COMMAND ${CMAKE_COMMAND} -E copy_directory
                "${CMAKE_SOURCE_DIR}/Prophesee/bin" "$<TARGET_FILE_DIR:${target}>"
            COMMENT "Copying bundled Metavision runtime DLLs next to ${target}")
    endif()

    if(EXISTS "${CMAKE_SOURCE_DIR}/Prophesee/lib/metavision/hal/plugins")
        add_custom_command(TARGET ${target} POST_BUILD
            COMMAND ${CMAKE_COMMAND} -E make_directory "$<TARGET_FILE_DIR:${target}>/hal_plugins"
            COMMAND ${CMAKE_COMMAND} -E copy_directory
                "${CMAKE_SOURCE_DIR}/Prophesee/lib/metavision/hal/plugins" "$<TARGET_FILE_DIR:${target}>/hal_plugins"
            COMMENT "Copying bundled Metavision HAL plugins next to ${target}")
    endif()

    if(EXISTS "${CMAKE_SOURCE_DIR}/Prophesee/bin/libusb-1.0.dll")
        add_custom_command(TARGET ${target} POST_BUILD
            COMMAND ${CMAKE_COMMAND} -E copy_if_different
                "${CMAKE_SOURCE_DIR}/Prophesee/bin/libusb-1.0.dll" "$<TARGET_FILE_DIR:${target}>/hal_plugins/"
            COMMENT "Copying libusb-1.0.dll next to ${target}'s hal_plugins")
    endif()

    if(EXISTS "${CMAKE_SOURCE_DIR}/opencv_world440.dll" OR EXISTS "${CMAKE_SOURCE_DIR}/opencv_world440d.dll")
        add_custom_command(TARGET ${target} POST_BUILD
            COMMAND ${CMAKE_COMMAND} -E copy_if_different
                "$<$<CONFIG:Debug>:${CMAKE_SOURCE_DIR}/opencv_world440d.dll>$<$<NOT:$<CONFIG:Debug>>:${CMAKE_SOURCE_DIR}/opencv_world440.dll>"
                "$<TARGET_FILE_DIR:${target}>"
            COMMENT "Copying bundled OpenCV DLL next to ${target}")
    endif()
endfunction()

add_subdirectory(EventProcessing.Core)
add_subdirectory(EventProcessing.Console)

if(EVENTCORE_HAVE_METAVISION)
    add_subdirectory(EventProcessing.DiagQt)
endif()
