#include "grover_cli.h"

int main(int argc, char *argv[]) {
	return tmfqs_examples::runGroverCli(
		argc,
		argv,
		"grover_normal",
		"normal",
		true);
}
