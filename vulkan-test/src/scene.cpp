#include "../include/scene.h"
#include "../include/vk_objects.h"

#include <iostream>
#include <memory>


Scene::Scene() {
	std::cout << this << std::endl;
	objects.push_back(new Plane(0, this, .3f, .3f, 0.0f, 0.0f, {1.0f, 0.0f, 0.0f}));
	objects.push_back(new Plane(1, this, .3f, .3f, 0.5f, 0.0f, {0.0f, 1.0f, 0.0f}));
}

void Scene::add_object(Object* object_ptr) {
	objects.push_back(object_ptr);
	// TO DO: verify unique id
}

void Scene::remove_object(uint8_t object_id) {
	for (int i = 0; i < objects.size(); i++) {
		if (objects[i]->get_id() == object_id) {
			// Release memory pointed by pointer-variable
			delete objects[i];
			// Delete vector record
			objects.erase(objects.begin() + i);
			return;
		}
	}
	std::cout << "object with id " << object_id << " not found" << std::endl;
}

Object* Scene::get_object_ptr(uint8_t object_id) {
	for (auto object : objects) {
		if (object->get_id() == object_id) return object;
	}
}

std::vector<Object*>* Scene::get_objects() {
	return &objects;
}

double Scene::triangle_area(Point a, Point b, Point c) {
	float A = sqrt((double)(b.x - a.x) * (b.x - a.x) + (b.y - a.y) * (b.y - a.y));
	float B = sqrt((double)(b.x - c.x) * (b.x - c.x) + (b.y - c.y) * (b.y - c.y));
	float C = sqrt((double)(a.x - c.x) * (a.x - c.x) + (a.y - c.y) * (a.y - c.y));

	// Heron's formula for area calculation
	// area = sqrt( s * (s-a) * (s-b) * (s-c))

	float s = (A + B + C) / 2;
	float perimeter = A + B + C;
	
	return sqrt(s * (s - A) * (s - B) * (s - C));
}

bool Scene::point_inside_triangle(Point point, std::vector<Point> triangle) {
	float A = triangle_area(triangle[0], triangle[1], triangle[2]);
	float A1 = triangle_area(point, triangle[1], triangle[2]);
	float A2 = triangle_area(triangle[0], point, triangle[2]);
	float A3 = triangle_area(triangle[0], triangle[1], point);
	
	float sum = A1 + A2 + A3;

	return abs(A - sum) < 1.0f / (window_width * 8.0f);
}

// assuming viewport normalized on y axis and camera looking at 0,0
void Scene::cursor_position_callback(int xpos, int ypos) {
	world_curson_pos_x = ((float)xpos / window_width -0.5f) * window_width / window_height;
	world_curson_pos_y = -((float)ypos / window_height -0.5f);

	// check if cursor is hovering an object
	int new_hovering_obj_id = -1; // reset

	for (int o = objects.size() - 1; o >= 0; o--) {
		std::vector<Vertex> vertices = objects[o]->get_vertices();
		std::vector<uint16_t> indices = objects[o]->get_indices();
		
		if (new_hovering_obj_id >= 0) break;

		for (int i = 0; i < indices.size(); i += 3) {
			std::vector<Point> triangle = {
				Point({ vertices[indices[i]].pos.x, vertices[indices[i]].pos.y }),
				Point({ vertices[indices[i + 1]].pos.x, vertices[indices[i + 1]].pos.y }),
				Point({ vertices[indices[i + 2]].pos.x, vertices[indices[i + 2]].pos.y })
			};

			// Cursor is hovering obj
			if (point_inside_triangle(
				Point({ world_curson_pos_x, world_curson_pos_y }),
				triangle
			)) {
				// invoke object hover enter event
				if (objects[o]->get_id() != hovering_obj_id) {
					objects[o]->on_hover_enter();
				}

				new_hovering_obj_id = objects[o]->get_id();
				break;
			}
		}
	}

	// invoke object hover leave event
	if (hovering_obj_id != new_hovering_obj_id && hovering_obj_id >= 0)
		get_object_ptr(hovering_obj_id)->on_hover_leave();

	hovering_obj_id = new_hovering_obj_id;

	//std::cout << new_hovering_obj_id << ", " << selected_obj_id << std::endl;

	// perform dragging
	if (dragging_obj_id >= 0)
		get_object_ptr(dragging_obj_id)->on_move(world_curson_pos_x, world_curson_pos_y);
}

void Scene::mouse_button_callback(int button, int action, int mods) {
	if (button == GLFW_MOUSE_BUTTON_RIGHT && action == GLFW_PRESS) {
		if (hovering_obj_id >= 0) {
			auto object = get_object_ptr(hovering_obj_id);
			if (!object->selectable())
				return;
		}
			
		if (selected_obj_id >= 0)
			get_object_ptr(selected_obj_id)->on_release();

		selected_obj_id = hovering_obj_id;

		if (selected_obj_id >= 0)
			get_object_ptr(selected_obj_id)->on_select();
	}

	if (button == GLFW_MOUSE_BUTTON_LEFT && action == GLFW_PRESS) {
		if (hovering_obj_id >= 0) {
			dragging_obj_id = hovering_obj_id;
			// start dragging
			get_object_ptr(dragging_obj_id)->on_move(world_curson_pos_x, world_curson_pos_y);
		}
	}

	if (button == GLFW_MOUSE_BUTTON_LEFT && action == GLFW_RELEASE) {
		if (dragging_obj_id >= 0)
			dragging_obj_id = -1;
	}
}


void Scene::window_resize_callback(unsigned int width, unsigned int height) {
	Scene::window_width = width;
	Scene::window_height = height;
}