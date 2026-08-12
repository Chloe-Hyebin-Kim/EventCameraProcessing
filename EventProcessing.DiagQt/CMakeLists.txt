# Qt(Widgets) 기반 Diagnostic Viewer. EventProcessing.Diag(MFC)의 Linux/크로스플랫폼 버전.
# 실시간 라이브 카메라 프리뷰가 목적이므로 EventProcessing.Core와 마찬가지로 Metavision SDK가
# 반드시 필요하다 (최상위 CMakeLists.txt에서 EVENTCORE_HAVE_METAVISION일 때만 이 서브디렉터리를 추가함).

set(CMAKE_AUTOMOC ON)

find_package(Qt6 QUIET COMPONENTS Widgets)
if(Qt6Widgets_FOUND)
    set(_qt_widgets_target Qt6::Widgets)
else()
    find_package(Qt5 REQUIRED COMPONENTS Widgets)
    set(_qt_widgets_target Qt5::Widgets)
endif()

add_executable(EventProcessing.DiagQt
    main.cpp
    MainWindow.cpp
    MainWindow.h
)

target_link_libraries(EventProcessing.DiagQt PRIVATE
    EventProcessing::Core
    ${_qt_widgets_target}
)

set_target_properties(EventProcessing.DiagQt PROPERTIES
    CXX_STANDARD 17
    CXX_STANDARD_REQUIRED ON
)
