cmake_minimum_required(VERSION 3.21)

if(NOT DEFINED CMAKE_CXX_COMPILER AND NOT DEFINED ENV{CXX})
	find_program(TMFQS_DEFAULT_CXX_COMPILER NAMES icpx g++)
	if(TMFQS_DEFAULT_CXX_COMPILER)
		get_filename_component(TMFQS_DEFAULT_CXX_COMPILER_NAME "${TMFQS_DEFAULT_CXX_COMPILER}" NAME)
		set(CMAKE_CXX_COMPILER "${TMFQS_DEFAULT_CXX_COMPILER}" CACHE FILEPATH "Default C++ compiler" FORCE)
		message(STATUS "No C++ compiler explicitly configured, defaulting to ${TMFQS_DEFAULT_CXX_COMPILER_NAME}")
	endif()
endif()

project(TMFQS LANGUAGES CXX)

include(GNUInstallDirs)
include(CMakePackageConfigHelpers)
include(CheckIPOSupported)

list(APPEND CMAKE_MODULE_PATH "${CMAKE_CURRENT_SOURCE_DIR}/cmake")

if(NOT CMAKE_CONFIGURATION_TYPES AND NOT CMAKE_BUILD_TYPE)
	set(default_build_type "RelWithDebInfo")
	set(CMAKE_BUILD_TYPE "${default_build_type}" CACHE STRING "Build type" FORCE)
	set_property(CACHE CMAKE_BUILD_TYPE PROPERTY STRINGS Debug Release RelWithDebInfo MinSizeRel)
	message(STATUS "No CMAKE_BUILD_TYPE specified, defaulting to ${default_build_type}")
endif()

if(NOT DEFINED BUILD_SHARED_LIBS)
	set(BUILD_SHARED_LIBS ON CACHE BOOL "Build shared libraries by default")
endif()

set(CMAKE_EXPORT_COMPILE_COMMANDS ON)
set(CMAKE_RUNTIME_OUTPUT_DIRECTORY "${PROJECT_BINARY_DIR}/bin")
set(CMAKE_LIBRARY_OUTPUT_DIRECTORY "${PROJECT_BINARY_DIR}/lib")
set(CMAKE_ARCHIVE_OUTPUT_DIRECTORY "${PROJECT_BINARY_DIR}/lib")
set(CMAKE_BUILD_RPATH_USE_ORIGIN ON)

set(TMFQS_BUILD_EXAMPLES ON CACHE BOOL "Build example executables")
set(TMFQS_BUILD_TESTS ON CACHE BOOL "Build integration tests")
set(TMFQS_BUILD_BENCHMARKS ON CACHE BOOL "Build benchmark executables")
set(TMFQS_WITH_BLOSC2 AUTO CACHE STRING "Enable Blosc2 support: AUTO, ON, or OFF")
set(TMFQS_WITH_ZFP AUTO CACHE STRING "Enable ZFP support: AUTO, ON, or OFF")
set(TMFQS_ENABLE_SANITIZERS OFF CACHE STRING "Sanitizers to enable: OFF or address,undefined")
set(TMFQS_WARNINGS_AS_ERRORS OFF CACHE BOOL "Treat warnings as errors")
set(TMFQS_ENABLE_IPO OFF CACHE BOOL "Enable interprocedural optimization")

set_property(CACHE TMFQS_WITH_BLOSC2 PROPERTY STRINGS AUTO ON OFF)
set_property(CACHE TMFQS_WITH_ZFP PROPERTY STRINGS AUTO ON OFF)
set_property(CACHE TMFQS_ENABLE_SANITIZERS PROPERTY STRINGS OFF "address,undefined")

function(tmfqs_validate_choice variable)
	set(options ${ARGN})
	list(FIND options "${${variable}}" match_index)
	if(match_index EQUAL -1)
		string(REPLACE ";" ", " allowed_values "${options}")
		message(FATAL_ERROR "${variable} must be one of: ${allowed_values}")
	endif()
endfunction()

function(tmfqs_enable_ipo target_name)
	if(NOT TMFQS_ENABLE_IPO)
		return()
	endif()

	check_ipo_supported(RESULT ipo_supported OUTPUT ipo_error)
	if(NOT ipo_supported)
		message(FATAL_ERROR "TMFQS_ENABLE_IPO=ON but IPO is not supported: ${ipo_error}")
	endif()

	set_property(TARGET "${target_name}" PROPERTY INTERPROCEDURAL_OPTIMIZATION TRUE)
endfunction()

function(tmfqs_configure_target target_name)
	target_link_libraries("${target_name}" PRIVATE tmfqs_project_options)
	set_target_properties(
		"${target_name}"
		PROPERTIES
			RUNTIME_OUTPUT_DIRECTORY "${CMAKE_RUNTIME_OUTPUT_DIRECTORY}"
			LIBRARY_OUTPUT_DIRECTORY "${CMAKE_LIBRARY_OUTPUT_DIRECTORY}"
			ARCHIVE_OUTPUT_DIRECTORY "${CMAKE_ARCHIVE_OUTPUT_DIRECTORY}")
	tmfqs_enable_ipo("${target_name}")
endfunction()

function(tmfqs_configure_runtime_target target_name)
	tmfqs_configure_target("${target_name}")
	if(UNIX AND NOT APPLE)
		set_property(TARGET "${target_name}" PROPERTY BUILD_RPATH "$ORIGIN/../lib")
		set_property(TARGET "${target_name}" PROPERTY INSTALL_RPATH "$ORIGIN/../${CMAKE_INSTALL_LIBDIR}")
	endif()
endfunction()

tmfqs_validate_choice(TMFQS_WITH_BLOSC2 AUTO ON OFF)
tmfqs_validate_choice(TMFQS_WITH_ZFP AUTO ON OFF)
tmfqs_validate_choice(TMFQS_ENABLE_SANITIZERS OFF "address,undefined")

add_library(tmfqs_project_options INTERFACE)
target_compile_features(tmfqs_project_options INTERFACE cxx_std_17)

if(CMAKE_CXX_COMPILER_ID MATCHES "GNU|Clang|AppleClang|IntelLLVM")
	target_compile_options(tmfqs_project_options INTERFACE -Wall -Wextra)
	if(TMFQS_WARNINGS_AS_ERRORS)
		target_compile_options(tmfqs_project_options INTERFACE -Werror)
	endif()
endif()

if(NOT TMFQS_ENABLE_SANITIZERS STREQUAL "OFF")
	if(NOT CMAKE_CXX_COMPILER_ID MATCHES "GNU|Clang|AppleClang|IntelLLVM")
		message(FATAL_ERROR "TMFQS_ENABLE_SANITIZERS requires a GCC- or Clang-compatible compiler")
	endif()
	target_compile_options(tmfqs_project_options INTERFACE -fsanitize=${TMFQS_ENABLE_SANITIZERS} -fno-omit-frame-pointer)
	target_link_options(tmfqs_project_options INTERFACE -fsanitize=${TMFQS_ENABLE_SANITIZERS})
endif()

set(TMFQS_HAS_BLOSC2 OFF)
if(NOT TMFQS_WITH_BLOSC2 STREQUAL "OFF")
	find_package(Blosc2 QUIET)
	if(Blosc2_FOUND)
		set(TMFQS_HAS_BLOSC2 ON)
	elseif(TMFQS_WITH_BLOSC2 STREQUAL "ON")
		message(FATAL_ERROR "TMFQS_WITH_BLOSC2=ON but Blosc2 was not found")
	endif()
endif()

set(TMFQS_HAS_ZFP OFF)
if(NOT TMFQS_WITH_ZFP STREQUAL "OFF")
	find_package(ZFP QUIET)
	if(ZFP_FOUND)
		set(TMFQS_HAS_ZFP ON)
	elseif(TMFQS_WITH_ZFP STREQUAL "ON")
		message(FATAL_ERROR "TMFQS_WITH_ZFP=ON but ZFP was not found")
	endif()
endif()

message(STATUS "TMFQS build examples: ${TMFQS_BUILD_EXAMPLES}")
message(STATUS "TMFQS build tests: ${TMFQS_BUILD_TESTS}")
message(STATUS "TMFQS build benchmarks: ${TMFQS_BUILD_BENCHMARKS}")
message(STATUS "TMFQS Blosc2 support: ${TMFQS_HAS_BLOSC2}")
message(STATUS "TMFQS ZFP support: ${TMFQS_HAS_ZFP}")

add_subdirectory(src)

if(TMFQS_BUILD_EXAMPLES)
	add_subdirectory(examples)
endif()

if(TMFQS_BUILD_TESTS)
	enable_testing()
	add_subdirectory(tests)
endif()

if(TMFQS_BUILD_BENCHMARKS)
	add_subdirectory(benchmarks)
endif()

configure_package_config_file(
	"${CMAKE_CURRENT_SOURCE_DIR}/cmake/TMFQSConfig.cmake.in"
	"${CMAKE_CURRENT_BINARY_DIR}/TMFQSConfig.cmake"
	INSTALL_DESTINATION "${CMAKE_INSTALL_LIBDIR}/cmake/TMFQS")

install(DIRECTORY "${CMAKE_CURRENT_SOURCE_DIR}/include/" DESTINATION "${CMAKE_INSTALL_INCLUDEDIR}")
install(
	EXPORT TMFQSTargets
	FILE TMFQSTargets.cmake
	NAMESPACE TMFQS::
	DESTINATION "${CMAKE_INSTALL_LIBDIR}/cmake/TMFQS")
install(
	FILES
		"${CMAKE_CURRENT_BINARY_DIR}/TMFQSConfig.cmake"
		"${CMAKE_CURRENT_SOURCE_DIR}/cmake/FindBlosc2.cmake"
		"${CMAKE_CURRENT_SOURCE_DIR}/cmake/FindZFP.cmake"
	DESTINATION "${CMAKE_INSTALL_LIBDIR}/cmake/TMFQS")

export(
	EXPORT TMFQSTargets
	FILE "${CMAKE_CURRENT_BINARY_DIR}/TMFQSTargets.cmake"
	NAMESPACE TMFQS::)
