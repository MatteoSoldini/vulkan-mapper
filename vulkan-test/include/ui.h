#pragma once

#include "scene.h"

class Scene;

class UI {
private:
	bool show_demo_window = false;
	Scene* pScene;
	GLFWwindow* window;

	void menu();
	void draw_menu_bar();
	std::string open_file_dialog();

public:
	UI(Scene* scene, GLFWwindow* window);

	void draw_ui();

	UI() = default;
};