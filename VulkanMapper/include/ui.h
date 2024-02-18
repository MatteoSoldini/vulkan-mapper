#pragma once

#include "scene.h"
#include "media_manager.h"
#include "vm_types.h"
#include <GLFW/glfw3.h>
#include "app.h"

class Scene;

class MediaManager;

struct Video;

class VulkanState;

class App;

class UI {
private:
	App* pApp;
	bool showImGuiDemoWindow = false;
	
	GLFWwindow* pWindow;

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
	UI(App* pApp, GLFWwindow* pWindow);

	void drawUi();

	UI() = default;
};