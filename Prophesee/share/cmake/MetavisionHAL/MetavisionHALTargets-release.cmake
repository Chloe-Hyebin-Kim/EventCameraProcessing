#----------------------------------------------------------------
# Generated CMake target import file for configuration "Release".
#----------------------------------------------------------------

# Commands may need to know the format version.
set(CMAKE_IMPORT_FILE_VERSION 1)

# Import target "Metavision::HAL" for configuration "Release"
set_property(TARGET Metavision::HAL APPEND PROPERTY IMPORTED_CONFIGURATIONS RELEASE)
set_target_properties(Metavision::HAL PROPERTIES
  IMPORTED_IMPLIB_RELEASE "${_IMPORT_PREFIX}/lib/metavision_hal.lib"
  IMPORTED_LOCATION_RELEASE "${_IMPORT_PREFIX}/bin/metavision_hal.dll"
  )

list(APPEND _cmake_import_check_targets Metavision::HAL )
list(APPEND _cmake_import_check_files_for_Metavision::HAL "${_IMPORT_PREFIX}/lib/metavision_hal.lib" "${_IMPORT_PREFIX}/bin/metavision_hal.dll" )

# Import target "Metavision::HAL_discovery" for configuration "Release"
set_property(TARGET Metavision::HAL_discovery APPEND PROPERTY IMPORTED_CONFIGURATIONS RELEASE)
set_target_properties(Metavision::HAL_discovery PROPERTIES
  IMPORTED_IMPLIB_RELEASE "${_IMPORT_PREFIX}/lib/metavision_hal_discovery.lib"
  IMPORTED_LOCATION_RELEASE "${_IMPORT_PREFIX}/bin/metavision_hal_discovery.dll"
  )

list(APPEND _cmake_import_check_targets Metavision::HAL_discovery )
list(APPEND _cmake_import_check_files_for_Metavision::HAL_discovery "${_IMPORT_PREFIX}/lib/metavision_hal_discovery.lib" "${_IMPORT_PREFIX}/bin/metavision_hal_discovery.dll" )

# Commands beyond this point should not need to know the version.
set(CMAKE_IMPORT_FILE_VERSION)
