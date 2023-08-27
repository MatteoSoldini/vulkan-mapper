#pragma once

#include "scene.h"

class Scene;

struct Viewport {
	VkDescriptorSet descriptor_set;
	float width;
	float height;
};

class UI {
private:
	bool show_demo_window = false;
	Scene* scene;
	GLFWwindow* window;

	void menu();
	void draw_menu_bar();
	std::string open_file_dialog();

public:
	UI(Scene* scene, GLFWwindow* window);

	void draw_ui(Viewport viewport);

	UI() = default;
};