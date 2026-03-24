find_path(Blosc2_INCLUDE_DIR NAMES blosc2.h)
find_library(Blosc2_LIBRARY NAMES blosc2)

include(FindPackageHandleStandardArgs)
find_package_handle_standard_args(
	Blosc2
	REQUIRED_VARS Blosc2_INCLUDE_DIR Blosc2_LIBRARY)

if(Blosc2_FOUND AND NOT TARGET Blosc2::Blosc2)
	add_library(Blosc2::Blosc2 UNKNOWN IMPORTED)
	set_target_properties(
		Blosc2::Blosc2
		PROPERTIES
			IMPORTED_LOCATION "${Blosc2_LIBRARY}"
			INTERFACE_INCLUDE_DIRECTORIES "${Blosc2_INCLUDE_DIR}")
endif()

mark_as_advanced(Blosc2_INCLUDE_DIR Blosc2_LIBRARY)
