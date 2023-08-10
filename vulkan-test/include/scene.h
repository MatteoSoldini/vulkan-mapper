#pragma once

#include <GLFW/glfw3.h>

#include <vector>
#include <memory>
#include "vk_objects.h"

class Object;

class Scene {
private:
	unsigned int window_width = 0;
	unsigned int window_height = 0;

	int hovering_obj_id = -1;	// mouse hover
	int selected_obj_id = -1;	// clicked
	int dragging_obj_id = -1;

	std::vector<Object*> objects;

public:
	Scene();

	void add_object(Object* object_ptr);
	void remove_object(uint8_t object_id);
	Object* get_object_ptr(uint8_t object_id);
	std::vector<Object*>* get_objects();
	double triangle_area(Point a, Point b, Point c);
	bool point_inside_triangle(Point point, std::vector<Point> triangle);
	void cursor_position_callback(int xpos, int ypos);
	void mouse_button_callback(int button, int action, int mods);
	void window_resize_callback(unsigned int width, unsigned int height);
};