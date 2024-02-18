#include <iostream>

// Load volk definitions once
#define VOLK_IMPLEMENTATION
#include <volk.h>
#include "include/app.h"

int main() {
	App app;

	try {
		app.run();
	} catch (const std::exception& e) {
		std::cerr << e.what() << std::endl;
		return EXIT_FAILURE;
	}

	return EXIT_SUCCESS;
}