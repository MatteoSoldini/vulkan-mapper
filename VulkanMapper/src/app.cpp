#include "../include/app.h"

App::App() {
	// constructors
	pVkState = new VulkanState(this);
	pScene = new Scene(this);
	pMediaManager = new MediaManager(this);
	pOutput = new VulkanOutput(this);
}

void App::run() {
	init();

	while (!close) {
		// update state
		pMediaManager->updateMedia();

		// draw windows
		pVkState->draw();

		if (outputMonitor >= 0) {
			pOutput->draw();
		}
	}

	cleanup();
}

void App::init() {
	pVkState->init();
}

void App::cleanup() {
	pMediaManager->cleanup();

	pVkState->cleanup();

	if (outputMonitor >= 0) {
		pOutput->cleanup();
	}
}


void App::setClose() {
	close = true;
}

void App::setOutputMonitor(int outputMonitor) {
	App::outputMonitor = outputMonitor;

	if (outputMonitor >= 0) {
		pOutput->init(outputMonitor);
	}
	else {
		pOutput->cleanup();
	}
}
