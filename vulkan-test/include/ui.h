#pragma once

#include "scene.h"
#include "media_manager.h"

class Scene;

class MediaManager;

struct Video;

class UI {
private:
	bool show_demo_window = false;
	VulkanEngine* pEngine;

	void planesMenu();
	void drawTopBar();
	void drawMediaManager();
	void drawPropertiesManager();
	void viewport();
	std::string openFileDialog();
	void drawVideoProperties(Video* pVideoState);

public:
	UI(VulkanEngine* pEngine);

	void drawUi();

	UI() = default;
};