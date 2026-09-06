#----------------------------------------------------------------
# Generated CMake target import file for configuration "Release".
#----------------------------------------------------------------

# Commands may need to know the format version.
set(CMAKE_IMPORT_FILE_VERSION 1)

# Import target "Metavision::PSEEHWLayer" for configuration "Release"
set_property(TARGET Metavision::PSEEHWLayer APPEND PROPERTY IMPORTED_CONFIGURATIONS RELEASE)
set_target_properties(Metavision::PSEEHWLayer PROPERTIES
  IMPORTED_IMPLIB_RELEASE "${_IMPORT_PREFIX}/lib/metavision/hal/plugins/metavision_psee_hw_layer.lib"
  IMPORTED_LOCATION_RELEASE "${_IMPORT_PREFIX}/lib/metavision/hal/plugins/metavision_psee_hw_layer.dll"
  )

list(APPEND _cmake_import_check_targets Metavision::PSEEHWLayer )
list(APPEND _cmake_import_check_files_for_Metavision::PSEEHWLayer "${_IMPORT_PREFIX}/lib/metavision/hal/plugins/metavision_psee_hw_layer.lib" "${_IMPORT_PREFIX}/lib/metavision/hal/plugins/metavision_psee_hw_layer.dll" )

# Commands beyond this point should not need to know the version.
set(CMAKE_IMPORT_FILE_VERSION)
