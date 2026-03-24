set(TMFQS_LIBRARY_SOURCES
	tmfqs/register/quantum_register.cpp
	tmfqs/gates/quantum_gate.cpp
	tmfqs/core/math.cpp
	tmfqs/core/random.cpp
	tmfqs/algorithms/operations.cpp
	tmfqs/algorithms/qft.cpp
	tmfqs/algorithms/grover.cpp
	tmfqs/storage/factory/state_backend_factory.cpp
	tmfqs/storage/factory/storage_strategy_registry.cpp
	tmfqs/storage/dense/dense_state_backend.cpp
	tmfqs/storage/blosc/blosc_codec.cpp
	tmfqs/storage/blosc/blosc_state_backend.cpp
	tmfqs/storage/zfp/zfp_codec.cpp
	tmfqs/storage/zfp/zfp_state_backend.cpp)

add_library(tmfqsfs ${TMFQS_LIBRARY_SOURCES})
add_library(TMFQS::tmfqsfs ALIAS tmfqsfs)

target_include_directories(
	tmfqsfs
	PUBLIC
		$<BUILD_INTERFACE:${PROJECT_SOURCE_DIR}/include>
		$<INSTALL_INTERFACE:${CMAKE_INSTALL_INCLUDEDIR}>
	PRIVATE
		${PROJECT_SOURCE_DIR}/src)

set_target_properties(
	tmfqsfs
	PROPERTIES
		OUTPUT_NAME tmfqsfs
		POSITION_INDEPENDENT_CODE ON)

target_compile_definitions(
	tmfqsfs
	PRIVATE
		$<$<BOOL:${TMFQS_HAS_BLOSC2}>:HAVE_BLOSC2>
		$<$<BOOL:${TMFQS_HAS_ZFP}>:HAVE_ZFP>)

target_link_libraries(
	tmfqsfs
	PRIVATE
		$<$<BOOL:${TMFQS_HAS_BLOSC2}>:Blosc2::Blosc2>
		$<$<BOOL:${TMFQS_HAS_ZFP}>:ZFP::ZFP>)

tmfqs_configure_target(tmfqsfs)

install(
	TARGETS tmfqsfs
	EXPORT TMFQSTargets
	RUNTIME DESTINATION "${CMAKE_INSTALL_BINDIR}"
	LIBRARY DESTINATION "${CMAKE_INSTALL_LIBDIR}"
	ARCHIVE DESTINATION "${CMAKE_INSTALL_LIBDIR}"
	INCLUDES DESTINATION "${CMAKE_INSTALL_INCLUDEDIR}")
