if (ANDROID)
    include(${CMAKE_CURRENT_LIST_DIR}/../../android/env.cmake)
endif (ANDROID)

find_package(MetavisionHAL REQUIRED QUIET)
find_package(hdf5_ecf REQUIRED QUIET)

if(NOT TARGET MetavisionSDK::stream)
    include("${CMAKE_CURRENT_LIST_DIR}/MetavisionSDK_streamTargets.cmake")
endif(NOT TARGET MetavisionSDK::stream)
