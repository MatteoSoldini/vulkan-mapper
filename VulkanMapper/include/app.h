#pragma once

#include "scene.h"
#include "media_manager.h"
#include "vk_state.h"
#include "ui.h"

class MediaManager;

class VulkanState;

class Scene;

class VulkanOutput;

class App {
private:
	int outputMonitor = -1;
	VulkanState* pVkState;
	Scene* pScene;
	MediaManager* pMediaManager;
	VulkanOutput* pOutput;
	bool close = false; // close app

public:
	App();

	// run app
	void run();

	void init();

	void cleanup();

	void setClose(); // call to close the app
	
	// output
	int getOutputMonitor() { return outputMonitor; };
	void setOutputMonitor(int outputMonitor);
	VulkanOutput* getOutput() { return pOutput; };

	// vulkan state
	VulkanState* getVulkanState() { return pVkState; };

	// scene
	Scene* getScene() { return pScene; };

	// media manager
	MediaManager* getMediaManager() { return pMediaManager; };

};