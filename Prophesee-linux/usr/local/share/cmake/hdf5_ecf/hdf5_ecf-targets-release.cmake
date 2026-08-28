#----------------------------------------------------------------
# Generated CMake target import file for configuration "Release".
#----------------------------------------------------------------

# Commands may need to know the format version.
set(CMAKE_IMPORT_FILE_VERSION 1)

# Import target "hdf5_ecf_codec" for configuration "Release"
set_property(TARGET hdf5_ecf_codec APPEND PROPERTY IMPORTED_CONFIGURATIONS RELEASE)
set_target_properties(hdf5_ecf_codec PROPERTIES
  IMPORTED_LOCATION_RELEASE "${_IMPORT_PREFIX}/lib/libhdf5_ecf_codec.so.1.0.1"
  IMPORTED_SONAME_RELEASE "libhdf5_ecf_codec.so.1"
  )

list(APPEND _cmake_import_check_targets hdf5_ecf_codec )
list(APPEND _cmake_import_check_files_for_hdf5_ecf_codec "${_IMPORT_PREFIX}/lib/libhdf5_ecf_codec.so.1.0.1" )

# Commands beyond this point should not need to know the version.
set(CMAKE_IMPORT_FILE_VERSION)
