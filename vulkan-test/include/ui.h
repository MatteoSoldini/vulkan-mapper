#pragma once

#include "scene.h"
#include "media_manager.h"

class Scene;

class MediaManager;

class UI {
private:
	bool show_demo_window = false;
	Scene* pScene;
	MediaManager* pMediaManager;
	GLFWwindow* window;

	void planesMenu();
	void drawTopBar();
	void drawMediaManager();
	std::string openFileDialog();

public:
	UI(Scene* scene, MediaManager* pMediaManager, GLFWwindow* window);

	void drawUi();

	UI() = default;
};