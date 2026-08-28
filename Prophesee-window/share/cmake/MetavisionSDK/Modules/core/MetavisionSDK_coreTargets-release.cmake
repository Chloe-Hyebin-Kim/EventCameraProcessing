#----------------------------------------------------------------
# Generated CMake target import file for configuration "Release".
#----------------------------------------------------------------

# Commands may need to know the format version.
set(CMAKE_IMPORT_FILE_VERSION 1)

# Import target "MetavisionSDK::core" for configuration "Release"
set_property(TARGET MetavisionSDK::core APPEND PROPERTY IMPORTED_CONFIGURATIONS RELEASE)
set_target_properties(MetavisionSDK::core PROPERTIES
  IMPORTED_IMPLIB_RELEASE "${_IMPORT_PREFIX}/lib/metavision_sdk_core.lib"
  IMPORTED_LOCATION_RELEASE "${_IMPORT_PREFIX}/bin/metavision_sdk_core.dll"
  )

list(APPEND _cmake_import_check_targets MetavisionSDK::core )
list(APPEND _cmake_import_check_files_for_MetavisionSDK::core "${_IMPORT_PREFIX}/lib/metavision_sdk_core.lib" "${_IMPORT_PREFIX}/bin/metavision_sdk_core.dll" )

# Commands beyond this point should not need to know the version.
set(CMAKE_IMPORT_FILE_VERSION)
