# Qt(Widgets) 기반 Diagnostic Viewer. Windows/Linux 공용 (MFC 버전은 더 이상 존재하지 않음).
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

eventcore_copy_windows_runtime_deps(EventProcessing.DiagQt)

# Qt DLL(및 platform 플러그인 등)은 windeployqt로 배포한다. 개발 중 실행 파일을 바로
# 실행해보고 싶다면 Qt bin\ 디렉터리를 PATH에 추가하거나 아래를 수동 실행한다:
#   windeployqt.exe <output_dir>\EventProcessing.DiagQt.exe
# --compiler-runtime을 붙여 vcruntime140.dll 등 MSVC 재배포 DLL까지 같이 챙기면, 출력 폴더
# 전체를 그대로 다른 PC(Qt도 Visual C++ 재배포 패키지도 안 깔려 있는)에 복사해도 바로 실행된다.
find_program(WINDEPLOYQT_EXECUTABLE windeployqt HINTS "${Qt6_DIR}/../../../bin" "${Qt5_DIR}/../../../bin")
if(WIN32 AND WINDEPLOYQT_EXECUTABLE)
    add_custom_command(TARGET EventProcessing.DiagQt POST_BUILD
        COMMAND "${WINDEPLOYQT_EXECUTABLE}" --no-translations --compiler-runtime "$<TARGET_FILE:EventProcessing.DiagQt>"
        COMMENT "Running windeployqt for EventProcessing.DiagQt")
endif()
