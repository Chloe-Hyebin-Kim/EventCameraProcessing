#----------------------------------------------------------------
# Generated CMake target import file for configuration "Release".
#----------------------------------------------------------------

# Commands may need to know the format version.
set(CMAKE_IMPORT_FILE_VERSION 1)

# Import target "MetavisionSDK::base" for configuration "Release"
set_property(TARGET MetavisionSDK::base APPEND PROPERTY IMPORTED_CONFIGURATIONS RELEASE)
set_target_properties(MetavisionSDK::base PROPERTIES
  IMPORTED_IMPLIB_RELEASE "${_IMPORT_PREFIX}/lib/metavision_sdk_base.lib"
  IMPORTED_LOCATION_RELEASE "${_IMPORT_PREFIX}/bin/metavision_sdk_base.dll"
  )

list(APPEND _cmake_import_check_targets MetavisionSDK::base )
list(APPEND _cmake_import_check_files_for_MetavisionSDK::base "${_IMPORT_PREFIX}/lib/metavision_sdk_base.lib" "${_IMPORT_PREFIX}/bin/metavision_sdk_base.dll" )

# Commands beyond this point should not need to know the version.
set(CMAKE_IMPORT_FILE_VERSION)
