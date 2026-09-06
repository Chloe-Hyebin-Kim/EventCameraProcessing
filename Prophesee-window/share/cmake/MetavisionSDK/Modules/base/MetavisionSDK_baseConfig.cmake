if (ANDROID)
    include(${CMAKE_CURRENT_LIST_DIR}/../../android/env.cmake)
endif (ANDROID)


if(NOT TARGET MetavisionSDK::base)
    include("${CMAKE_CURRENT_LIST_DIR}/MetavisionSDK_baseTargets.cmake")
endif(NOT TARGET MetavisionSDK::base)
