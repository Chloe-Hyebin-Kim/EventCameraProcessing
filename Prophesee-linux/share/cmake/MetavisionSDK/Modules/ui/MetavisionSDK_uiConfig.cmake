if (ANDROID)
    include(${CMAKE_CURRENT_LIST_DIR}/../../android/env.cmake)
endif (ANDROID)

set(OpenGL_GL_PREFERENCE GLVND)
find_package(OpenCV COMPONENTS core REQUIRED QUIET)
find_package(OpenGL REQUIRED QUIET)
find_package(GLEW REQUIRED QUIET)
find_package(glfw3 REQUIRED QUIET)

if(NOT TARGET MetavisionSDK::ui)
    include("${CMAKE_CURRENT_LIST_DIR}/MetavisionSDK_uiTargets.cmake")
endif(NOT TARGET MetavisionSDK::ui)
