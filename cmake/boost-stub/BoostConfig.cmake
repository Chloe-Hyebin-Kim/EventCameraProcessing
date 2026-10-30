# Boost가 설치되지 않은 Windows PC용 대체(stub) Boost CMake 패키지.
#
# 리포에 번들된 Metavision SDK(Prophesee-window/)의 MetavisionSDK_coreConfig.cmake는
# find_package(Boost COMPONENTS timer REQUIRED)를 호출하고 MetavisionSDK::core의 링크 인터페이스에
# Boost::boost / Boost::timer를 넣어두지만, 이 프로젝트가 실제로는 Boost를 쓰지 않는다:
#  - Prophesee-window\bin\*.dll 중 어느 것도 Boost DLL을 import하지 않음(objdump -p로 확인).
#  - 이 프로젝트 소스가 include하는 Metavision 헤더(sdk/stream/camera.h 등)를 따라가도
#    boost/ 헤더는 하나도 포함되지 않음.
# 그래서 Boost를 통째로 번들(헤더만 100MB+, 라이브러리는 MSVC 툴셋별)하는 대신, SDK의 CMake 설정이
# 요구하는 타깃만 빈 INTERFACE 타깃으로 만들어 configure가 통과되게 한다.
# 실제 Boost가 설치되어 있으면 CMakeLists.txt가 그쪽을 먼저 찾아 쓰고 이 파일은 쓰이지 않는다.
#
# 나중에 boost/ 헤더가 필요한 Metavision 헤더(예: sdk/core/utils/... 의 timing_profiler,
# property_tree를 쓰는 preprocessors)를 include하게 되면 컴파일 단계에서 "boost/...hpp를 찾을 수
# 없음" 에러가 나므로, 그때는 실제 Boost를 설치하면 된다(README 참고).

set(Boost_VERSION 0.0.0)
set(Boost_VERSION_STRING 0.0.0)
set(Boost_INCLUDE_DIRS "")
set(Boost_LIBRARIES "")

if(NOT TARGET Boost::boost)
    add_library(Boost::boost INTERFACE IMPORTED)
endif()
if(NOT TARGET Boost::headers)
    add_library(Boost::headers INTERFACE IMPORTED)
endif()

foreach(_comp IN LISTS Boost_FIND_COMPONENTS)
    if(NOT TARGET Boost::${_comp})
        add_library(Boost::${_comp} INTERFACE IMPORTED)
    endif()
    set(Boost_${_comp}_FOUND TRUE)
endforeach()

set(Boost_FOUND TRUE)
set(EVENTCORE_BOOST_IS_STUB TRUE)
