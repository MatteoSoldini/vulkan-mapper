#pragma once

#include "scene.h"

class Scene;

class UI {
private:
	bool show_demo_window = false;
	Scene* scene;
	GLFWwindow* window;

public:
	UI(Scene* scene, GLFWwindow* window);
	void draw_ui();
	void draw_function_window();
	void draw_menu_bar();
	std::string open_file_dialog();

	UI() = default;
};