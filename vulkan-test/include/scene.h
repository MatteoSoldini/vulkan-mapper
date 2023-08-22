#pragma once

#include <GLFW/glfw3.h>

#include <vector>
#include <memory>
#include "vk_objects.h"
#include "vk_engine.h"

class Object;

class VulkanEngine;

class Scene {
private:
	unsigned int window_width = 0;
	unsigned int window_height = 0;

	int hovering_obj_id = -1;	// mouse hover
	int selected_obj_id = -1;	// clicked
	int dragging_obj_id = -1;

	std::vector<Object*> objects;

	VulkanEngine* engine;

public:
	Scene(VulkanEngine* engine);

	int get_selected_obj_id();
	int get_hovering_obj_id();

	uint8_t add_object(Object* object_ptr);
	void remove_object(uint8_t object_id);
	Object* get_object_ptr(uint8_t object_id);
	std::vector<Object*>* get_objects();
	std::vector<uint8_t> get_ids();
	double triangle_area(glm::vec2 a, glm::vec2 b, glm::vec2 c);
	bool point_inside_triangle(glm::vec2 point, std::vector<glm::vec2> triangle);
	void cursor_position_callback(int xpos, int ypos);
	void mouse_button_callback(int button, int action, int mods);
	void window_resize_callback(unsigned int width, unsigned int height);
	VulkanEngine* get_engine_ptr() {
		return engine;
	};
};