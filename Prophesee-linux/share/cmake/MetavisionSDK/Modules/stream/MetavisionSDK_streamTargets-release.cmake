#----------------------------------------------------------------
# Generated CMake target import file for configuration "Release".
#----------------------------------------------------------------

# Commands may need to know the format version.
set(CMAKE_IMPORT_FILE_VERSION 1)

# Import target "MetavisionSDK::stream" for configuration "Release"
set_property(TARGET MetavisionSDK::stream APPEND PROPERTY IMPORTED_CONFIGURATIONS RELEASE)
set_target_properties(MetavisionSDK::stream PROPERTIES
  IMPORTED_LINK_DEPENDENT_LIBRARIES_RELEASE "hdf5_ecf_codec"
  IMPORTED_LOCATION_RELEASE "${_IMPORT_PREFIX}/lib/libmetavision_sdk_stream.so.5.2.0"
  IMPORTED_SONAME_RELEASE "libmetavision_sdk_stream.so.5"
  )

list(APPEND _cmake_import_check_targets MetavisionSDK::stream )
list(APPEND _cmake_import_check_files_for_MetavisionSDK::stream "${_IMPORT_PREFIX}/lib/libmetavision_sdk_stream.so.5.2.0" )

# Commands beyond this point should not need to know the version.
set(CMAKE_IMPORT_FILE_VERSION)
