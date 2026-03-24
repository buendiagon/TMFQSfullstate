find_path(ZFP_INCLUDE_DIR NAMES zfp.h)
find_library(ZFP_LIBRARY NAMES zfp)

include(FindPackageHandleStandardArgs)
find_package_handle_standard_args(
	ZFP
	REQUIRED_VARS ZFP_INCLUDE_DIR ZFP_LIBRARY)

if(ZFP_FOUND AND NOT TARGET ZFP::ZFP)
	add_library(ZFP::ZFP UNKNOWN IMPORTED)
	set_target_properties(
		ZFP::ZFP
		PROPERTIES
			IMPORTED_LOCATION "${ZFP_LIBRARY}"
			INTERFACE_INCLUDE_DIRECTORIES "${ZFP_INCLUDE_DIR}")
endif()

mark_as_advanced(ZFP_INCLUDE_DIR ZFP_LIBRARY)
