#pragma once

#include "scene.h"
#include "media_manager.h"

class Scene;

class MediaManager;

class UI {
private:
	bool show_demo_window = false;
	VulkanEngine* pEngine;

	void planesMenu();
	void drawTopBar();
	void drawMediaManager();
	void viewport();
	std::string openFileDialog();

public:
	UI(VulkanEngine* pEngine);

	void drawUi();

	UI() = default;
};