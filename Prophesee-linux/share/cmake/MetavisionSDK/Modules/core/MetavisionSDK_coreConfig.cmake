if (ANDROID)
    include(${CMAKE_CURRENT_LIST_DIR}/../../android/env.cmake)
endif (ANDROID)

find_package(OpenCV COMPONENTS core imgproc highgui videoio REQUIRED QUIET)
find_package(Boost COMPONENTS timer REQUIRED QUIET)
find_package(Threads REQUIRED QUIET)

if(NOT TARGET MetavisionSDK::core)
    include("${CMAKE_CURRENT_LIST_DIR}/MetavisionSDK_coreTargets.cmake")
endif(NOT TARGET MetavisionSDK::core)
