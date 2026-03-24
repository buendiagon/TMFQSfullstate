set(TMFQS_EXAMPLES
	applyHadamard
	applyControlledPhaseShift
	applyControlledNot
	qft
	qftG
	grover
	getSumOfProbabilities)

foreach(example_name IN LISTS TMFQS_EXAMPLES)
	add_executable("${example_name}" "${example_name}.cpp")
	target_link_libraries("${example_name}" PRIVATE tmfqsfs)
	tmfqs_configure_runtime_target("${example_name}")
endforeach()
