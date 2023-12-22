#pragma once

#include "scene.h"
#include "media_manager.h"
#include "vm_types.h"

class Scene;

class MediaManager;

struct Video;

class VulkanState;

class UI {
private:
	bool showImGuiDemoWindow = false;
	
	VulkanState* pEngine;


	bool selectedMedia = false;
	MediaId_t selectedMediaId = 0;
	void selectMedia(MediaId_t mediaId);
	void deselectMedia();

	void planesMenu();
	void drawTopBar();
	void drawMediaManager();
	void drawPropertiesManager();
	void viewport();
	std::string openFileDialog();
	void drawVideoProperties(Video* pVideo);

public:
	UI(VulkanState* pEngine);

	void drawUi();

	UI() = default;
};